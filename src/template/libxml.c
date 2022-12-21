#ifndef KILITE_AMALGAMATION
#include "common.h"
#include "lib.h"
#endif

/* XmlDom */

enum {
    XMLDOM_EDECL    = -4,
    XMLDOM_EREF     = -3,
    XMLDOM_ECLOSE   = -2,
    XMLDOM_ESYNTAX  = -1,
    XMLDOM_OK       =  0,
};

enum {
    XMLDOM_ELEMENT_NODE                 =  1,
    XMLDOM_ATTRIBUTE_NODE               =  2,
    XMLDOM_TEXT_NODE                    =  3,
    XMLDOM_CDATA_SECTION_NODE           =  4,
    XMLDOM_ENTITY_REFERENCE_NODE        =  5,
    XMLDOM_ENTITY_NODE                  =  6,
    XMLDOM_PROCESSING_INSTRUCTION_NODE  =  7,
    XMLDOM_COMMENT_NODE                 =  8,
    XMLDOM_DOCUMENT_NODE                =  9,
    XMLDOM_DOCUMENT_TYPE_NODE           = 10,
    XMLDOM_DOCUMENT_FRAGMENT_NODE       = 11,
    XMLDOM_NOTATION_NODE                = 12,

    XMLDOM_XMLDECL_NODE,
    XMLDOM_DOCUMENT_POSITION_CONTAINED_BY,
    XMLDOM_DOCUMENT_POSITION_CONTAINS,
    XMLDOM_DOCUMENT_POSITION_DISCONNECTED,
    XMLDOM_DOCUMENT_POSITION_FOLLOWING,
    XMLDOM_DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC,
    XMLDOM_DOCUMENT_POSITION_PRECEDING,
};

typedef struct xmlctx {
    int err;
    int line;
    int pos;
    int depth;
    int id;
    int normalize_space;
} xmlctx;

#define is_whitespace(x) ((x) == ' ' || (x) == '\t' || (x) == '\r' || (x) == '\n')
#define skip_whitespace(p) do { while (is_whitespace(*p)) { if (*p == '\n') { ++(xctx->line); (xctx->pos) = -1; } ++p; ++(xctx->pos); } } while (0)
#define move_next(p) do { if (*p == '\n') { ++(xctx->line), (xctx->pos) = -1;} ++p; ++(xctx->pos); } while (0)
#define check_error(err, condition, code) do { if (condition) { errset_wuth_ret(err, code); } } while (0)
#define syntax_error(err, condition) check_error(err, condition, XMLDOM_ESYNTAX)
#define end_of_text_error(err, p) syntax_error(err, *p == 0)
#define errset_wuth_ret(err, msg) do { err = msg; if (0) { printf("[%d] err set %s\n", __LINE__, #msg); } return p; } while (0)
static const char *parse_doc(vmctx *ctx, vmfrm *lex, vmvar *doc, vmobj *nsmap, vmvar *r, const char *p, xmlctx *xctx);
static int XPath_evaluate(vmctx *ctx, vmfrm *lex, vmvar *r, int ac);

static int xmldom_error(vmctx *ctx, int err, int line, int pos)
{
    char buf[256] = {0};
    switch (err) {
    case XMLDOM_EDECL:
        sprintf(buf, "Xml declaration location error at %d:%d", line, pos);
        break;
    case XMLDOM_EREF:
        sprintf(buf, "Xml invalid reference error at %d:%d", line, pos);
        break;
    case XMLDOM_ECLOSE:
        sprintf(buf, "Xml invalid close tag error at %d:%d", line, pos);
        break;
    case XMLDOM_ESYNTAX:
        sprintf(buf, "Xml syntax error at %d:%d", line, pos);
        break;
    default:
        sprintf(buf, "Xml parse error at %d:%d", line, pos);
        break;
    }
    return throw_system_exception(__LINE__, ctx, EXCEPT_XML_ERROR, buf);
}

static int is_same_tagname(const char *t, int tlen, const char *c, int clen)
{
    if (tlen != clen) {
        return 0;
    }
    for (int i = 0; i < tlen; ++i) {
        if (t[i] != c[i]) {
            return 0;
        }
    }
    return 1;
}

static const char *gen_intval(int *v, int *err, const char *s, int radix)
{
    int vv = 0;
    while (*s && *s != ';') {
        int add = 0;
        if ('0' <= *s && *s <= '9') {
            add = (*s - '0');
        } else if ('a' <= *s && *s <= 'f') {
            add = (*s - 'a' + 10);
        } else if ('A' <= *s && *s <= 'F') {
            add = (*s - 'A' + 10);
        } else {
            *err = XMLDOM_EREF;
            break;
        }
        if (radix <= add) {
            *err = XMLDOM_EREF;
            break;
        }
        vv = vv * radix + add;
        ++s;
    }
    if (*s == ';') ++s;
    *v = vv;
    return s;
}

static vmvar *make_pure_str_obj(vmctx *ctx, int *err, const char *s, int len)
{
    char *buf = alloca(len + 2);
    strncpy(buf, s, len);
    buf[len] = 0;
    return alcvar_str(ctx, buf);
}

static vmvar *make_str_obj(vmctx *ctx, int *err, const char *s, int len)
{
    char *buf = alloca(len + 2);
    char *p = buf;
    for (int i = 0; i < len; ++i) {
        if (*s != '&') {
            *p++ = *s++;
            continue;
        }
        ++s;
        if (*s == '#') {
            const char *ss = s;
            int v, x = (*++s == 'x');
            s = gen_intval(&v, err, x ? s+1 : s, x ? 16 : 10);
            if (*err < 0) break;
            len -= (s - ss);
            *p++ = v;
            continue;
        }
        #define xml_make_ref(condition, ch, count) if (condition) { \
            *p++ = ch; s += count; len -= count; \
            continue; \
        } \
        /**/
        xml_make_ref((*s == 'a' && *(s+1) == 'm' && *(s+2) == 'p' && *(s+3) == ';'), '&', 4);
        xml_make_ref((*s == 'l' && *(s+1) == 't' && *(s+2) == ';'), '<', 3);
        xml_make_ref((*s == 'g' && *(s+1) == 't' && *(s+2) == ';'), '>', 3);
        xml_make_ref((*s == 'q' && *(s+1) == 'u' && *(s+2) == 'o' && *(s+3) == 't' && *(s+4) == ';'), '"', 5);
        xml_make_ref((*s == 'a' && *(s+1) == 'p' && *(s+2) == 'o' && *(s+3) == 's' && *(s+4) == ';'), '\'', 5);
        #undef xml_make_ref
        *err = XMLDOM_EREF;
        break;
    }
    *p = 0;
    return alcvar_str(ctx, buf);
}

static void set_attrs(vmctx *ctx, int *err, vmobj *attrs, const char *n, int nlen, const char *v, int vlen)
{
    vmvar *value = make_str_obj(ctx, err, v, vlen);
    if (*err < 0) return;
    char *key = alloca(nlen + 2);
    strncpy(key, n, nlen);
    key[nlen] = 0;
    hashmap_set(ctx, attrs, key, value);
}

static const char *parse_attrs(vmctx *ctx, vmobj *nsmap, vmobj *attrs, const char *p, xmlctx *xctx)
{
    const char *attrn = p;
    while (!is_whitespace(*p)) {
        end_of_text_error((xctx->err), p);
        if (*p == '=') break;
        move_next(p);
    }
    int attrnlen = p - attrn;
    skip_whitespace(p);
    syntax_error((xctx->err), (*p != '='));
    move_next(p);
    skip_whitespace(p);
    syntax_error((xctx->err), (*p != '"' && *p != '\''));
    char br = *p;
    move_next(p);
    const char *attrv = p;
    while (*p != br) {
        end_of_text_error((xctx->err), p);
        if (*p == '\\') { move_next(p); }
        move_next(p);
    }
    if (strncmp(attrn, "xmlns", 5) == 0 && (*(attrn + 5) == '=' || is_whitespace(*(attrn+5)))) {
        vmvar *value = make_str_obj(ctx, &(xctx->err), attrv, p - attrv);
        array_set(ctx, nsmap, 0, value);
    } else if (strncmp(attrn, "xmlns:", 6) == 0) {
        set_attrs(ctx, &(xctx->err), nsmap, attrn + 6, attrnlen - 6, attrv, p - attrv);
    } else {
        set_attrs(ctx, &(xctx->err), attrs, attrn, attrnlen, attrv, p - attrv);
    }
    if (xctx->err < 0) return p;
    if (*p == br) { move_next(p); }
    return p;
}

static const char *parse_comment(vmctx *ctx, vmvar *r, const char *p, xmlctx *xctx)
{
    const char *s = p;
    while (*p) {
        move_next(p);
        if (*p == '-' && *(p+1) == '-' && *(p+2) == '>') {
            p += 2;
            break;
        }
    }
    syntax_error((xctx->err), (*p != '>'));
    vmvar *comment = make_pure_str_obj(ctx, &(xctx->err), s, p - s - 2);
    hashmap_set(ctx, r->o, "nodeName", alcvar_str(ctx, "#comment"));
    hashmap_set(ctx, r->o, "nodeValue", comment);
    hashmap_set(ctx, r->o, "comment", comment);
    return p;
}

static const char *parse_pi(vmctx *ctx, vmobj *nsmap, vmvar *doc, vmvar *r, const char *p, xmlctx *xctx, int *xmldecl)
{
    const char *s = p;
    while (!is_whitespace(*p)) {
        end_of_text_error((xctx->err), p);
        move_next(p);
    }
    if (s != p) {
        vmvar *target = make_str_obj(ctx, &(xctx->err), s, p - s);
        if (xctx->err < 0) return p;
        if (strcmp(target->s->hd, "xml") == 0) {
            *xmldecl = 1;
        }
        hashmap_set(ctx, r->o, "target", target);
        hashmap_set(ctx, r->o, "nodeName", target);
    }
    skip_whitespace(p);
    if (*p == '?') {
        move_next(p);
        syntax_error((xctx->err), (*p != '>'));
        return p;
    }
    vmobj *attrs = alcobj(ctx);
    while (1) {
        skip_whitespace(p);
        end_of_text_error((xctx->err), p);
        if (*p == '?') break;
        p = parse_attrs(ctx, nsmap, attrs, p, xctx);
        if (xctx->err < 0) return p;
    }
    if (*xmldecl) {
        vmvar *version = hashmap_search(attrs, "version");
        if (!version) version = alcvar_str(ctx, "1.0");
        hashmap_set(ctx, doc->o, "xmlVersion", version);
        vmvar *encoding = hashmap_search(attrs, "encoding");
        if (!encoding) encoding = alcvar_str(ctx, "UTF-8");
        hashmap_set(ctx, doc->o, "xmlEncoding", encoding);
        vmvar *standalone = hashmap_search(attrs, "standalone");
        int sv = !standalone || (standalone->t == VAR_STR && strcmp(standalone->s->hd, "yes") == 0);
        standalone = alcvar_int64(ctx, sv, 0);
        hashmap_set(ctx, doc->o, "xmlStandalone", standalone);
    }

    vmvar *data = alcvar_obj(ctx, attrs);
    hashmap_set(ctx, r->o, "data", data);
    hashmap_set(ctx, r->o, "nodeValue", data);
    move_next(p);
    return p;
}

static const char *get_namespace_uri(vmctx *ctx, vmobj *nsmap, const char *prefix)
{
    vmvar *ns = NULL;
    if (prefix) {
        ns = hashmap_search(nsmap, prefix);
    }
    if (!ns && nsmap->ary) {
        ns = nsmap->ary[0];
    }
    if (ns && ns->t == VAR_STR) {
        return ns->s->hd;
    }
    return "";
}

static const char *parse_node(vmctx *ctx, vmfrm *lex, vmvar *doc, vmobj *nsmap, vmvar *r, const char *p, xmlctx *xctx)
{
    skip_whitespace(p);
    end_of_text_error((xctx->err), p);

    const char *tag = p;
    int taglen = 0;
    const char *ns = p;
    int nslen = 0;
    while (!is_whitespace(*p)) {
        end_of_text_error((xctx->err), p);
        if (*p == '/' || *p == '>') break;
        if (*p == ':') {
            ns = tag; nslen = taglen;
            move_next(p);
            tag = p; taglen = 0;
        } else {
            move_next(p);
            ++taglen;
        }
    }
    syntax_error((xctx->err), (taglen == 0));

    KL_SET_PROPERTY_I(r->o, "isXmlNode", 1);
    vmvar *lname = make_str_obj(ctx, &(xctx->err), tag, taglen);
    if (xctx->err < 0) return p;
    hashmap_set(ctx, r->o, "localName", lname);
    vmvar *nsname = NULL;
    if (nslen > 0) {
        nsname = make_str_obj(ctx, &(xctx->err), ns, nslen);
        if (xctx->err < 0) return p;
        hashmap_set(ctx, r->o, "prefix", nsname);
        vmvar *tname = make_str_obj(ctx, &(xctx->err), ns, nslen + taglen + 1);
        if (xctx->err < 0) return p;
        hashmap_set(ctx, r->o, "tagName", tname);
        hashmap_set(ctx, r->o, "qName", tname);
        hashmap_set(ctx, r->o, "nodeName", tname);
    } else {
        hashmap_set(ctx, r->o, "prefix", alcvar_str(ctx, ""));
        hashmap_set(ctx, r->o, "tagName", lname);
        hashmap_set(ctx, r->o, "qName", lname);
        hashmap_set(ctx, r->o, "nodeName", lname);
    }
    skip_whitespace(p);

    vmobj *attrs = alcobj(ctx);
    if (*p != '/' && *p != '>') {
        while (1) {
            skip_whitespace(p);
            end_of_text_error((xctx->err), p);
            if (*p == '/' || *p == '>') break;
            p = parse_attrs(ctx, nsmap, attrs, p, xctx);
            if (xctx->err < 0) return p;
        }
    }
    hashmap_set(ctx, r->o, "attributes", alcvar_obj(ctx, attrs));

    if (nsname) {
        const char *uri = get_namespace_uri(ctx, nsmap, nsname->s->hd);
        hashmap_set(ctx, r->o, "namespaceURI", alcvar_str(ctx, uri));
    } else {
        const char *uri = get_namespace_uri(ctx, nsmap, NULL);
        hashmap_set(ctx, r->o, "namespaceURI", alcvar_str(ctx, uri));
    }

    if (*p == '/') { move_next(p); return p; }
    if (*p == '>') { move_next(p); }
    xctx->depth++;
    p = parse_doc(ctx, lex, doc, nsmap, r, p, xctx);
    xctx->depth--;
    if (xctx->err < 0) return p;
    syntax_error((xctx->err), (*p != '/'));
    move_next(p);

    const char *ctag = p;
    int ctaglen = 0;
    const char *cns = p;
    int cnslen = 0;
    while (!is_whitespace(*p)) {
        end_of_text_error((xctx->err), p);
        if (*p == '>') break;
        if (*p == ':') {
            cns = ctag; cnslen = ctaglen;
            move_next(p);
            ctag = p; ctaglen = 0;
        } else {
            move_next(p);
            ++ctaglen;
        }
    }
    skip_whitespace(p);

    if (!is_same_tagname(tag, taglen, ctag, ctaglen)) {
        errset_wuth_ret((xctx->err), XMLDOM_ECLOSE);
    }
    if (nslen > 0) {
        if (!is_same_tagname(ns, nslen, cns, cnslen)) {
            errset_wuth_ret((xctx->err), XMLDOM_ECLOSE);
        }
    }
    return p;
}

static vmvar *XmlDom_checkid(vmvar *node)
{
    vmvar *attr = hashmap_search(node->o, "attributes");
    if (attr && attr->t == VAR_OBJ) {
        return hashmap_search(attr->o, "id");
    }
    return NULL;
}

static vmvar *XmlDom_getElementById_Node(vmvar *node, const char *idvalue)
{
    vmvar *id = XmlDom_checkid(node);
    if (id && id->t == VAR_STR && strcmp(id->s->hd, idvalue) == 0) {
        return node;
    }
    vmobj *c = node->o;
    for (int i = 0, n = c->idxsz; i < n; i++) {
        vmvar *v = XmlDom_getElementById_Node(c->ary[i], idvalue);
        if (v) {
            return v;
        }
    }
    return NULL;
}

static int XmlDom_getElementById(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    DEF_ARG(a1, 1, VAR_STR);
    vmvar *found = XmlDom_getElementById_Node(a0, a1->s->hd);
    if (found && found->t == VAR_OBJ) {
        SET_OBJ(r, found->o);
    } else {
        SET_I64(r, 0);
    }
    return 0;
}

static void XmlDom_getElementsByTagName_Node(vmctx *ctx, vmobj *nodes, vmvar *node, const char *name)
{
    vmvar *tagName = hashmap_search(node->o, "tagName");
    if (tagName && tagName->t == VAR_STR && strcmp(tagName->s->hd, name) == 0) {
        array_push(ctx, nodes, node);
    }
    vmobj *c = node->o;
    for (int i = 0, n = c->idxsz; i < n; i++) {
        XmlDom_getElementsByTagName_Node(ctx, nodes, c->ary[i], name);
    }
}

static int XmlDom_getElementsByTagName(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    DEF_ARG(a1, 1, VAR_STR);
    vmobj *nodes = alcobj(ctx);
    XmlDom_getElementsByTagName_Node(ctx, nodes, a0, a1->s->hd);
    SET_OBJ(r, nodes);
    return 0;
}

static int XmlDom_removeNode(vmctx *ctx, vmvar *parent, vmobj *node)
{
    /* remove from the parent */
    if (parent && parent->t == VAR_OBJ) {
        vmobj *po = parent->o;
        array_remove_obj(po, node);
        vmvar *children = hashmap_search(po, "children");
        if (children && children->t == VAR_OBJ) {
            children->o->idxsz = 0; // clear nodes.
            for (int i = 0, n = children->o->idxsz; i < n; i++) {
                array_push(ctx, children->o, po->ary[i]);
            }
        }
        if (po->idxsz > 0) {
            KL_SET_PROPERTY(po, firstChild, po->ary[0]);
            KL_SET_PROPERTY(po, lastNode, po->ary[po->idxsz - 1]);
        }
    }

    /* adjustment of siblings */
    vmvar *prev = hashmap_search(node, "previousSibling");
    vmvar *next = hashmap_search(node, "nextSibling");
    if (prev && prev->t == VAR_OBJ) {
        KL_SET_PROPERTY(prev->o, nextSibling, next);
    }
    if (next && next->t == VAR_OBJ) {
        KL_SET_PROPERTY(next->o, previousSibling, prev);
    }

    /* remove this from tree, but child nodes will still be held. */
    hashmap_remove(ctx, node, "parentNode");
    hashmap_remove(ctx, node, "previousSibling");
    hashmap_remove(ctx, node, "nextSibling");

    return 0;
}

static int XmlDom_remove(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    vmobj *node = a0->o;
    vmvar *parent = hashmap_search(node, "parentNode");

    XmlDom_removeNode(ctx, parent, node);

    SET_I64(r, 0);
    return 0;
}

static int XmlDom_insertBefore(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(parent, 0, VAR_OBJ);
    DEF_ARG(a1, 1, VAR_OBJ);
    DEF_ARG(a2, 2, VAR_OBJ);
    vmobj *newnode = a1->o;
    vmobj *refnode = a2->o;

    /* append to the parent */
    KL_SET_PROPERTY_O(newnode, parentNode, parent->o);
    vmobj *po = parent->o;
    if (!array_insert_before_obj(ctx, po, refnode, newnode)) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_XML_ERROR, "Reference node not found");
    }
    vmvar *children = hashmap_search(po, "children");
    if (children && children->t == VAR_OBJ) {
        children->o->idxsz = 0; // clear nodes.
        for (int i = 0, n = children->o->idxsz; i < n; i++) {
            array_push(ctx, children->o, po->ary[i]);
        }
    }
    if (po->idxsz > 0) {
        KL_SET_PROPERTY(po, firstChild, po->ary[0]);
        KL_SET_PROPERTY(po, lastNode, po->ary[po->idxsz - 1]);
    }

    /* adjustment of siblings */
    vmvar *prev = hashmap_search(refnode, "previousSibling");
    if (prev && prev->t == VAR_OBJ) {
        KL_SET_PROPERTY_O(prev->o, nextSibling, newnode);
        KL_SET_PROPERTY(newnode, previousSibling, prev);
    }
    KL_SET_PROPERTY_O(refnode, previousSibling, newnode);
    KL_SET_PROPERTY_O(newnode, nextSibling, refnode);

    return 0;
}

static int XmlDom_insertAfter(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(parent, 0, VAR_OBJ);
    DEF_ARG(a1, 1, VAR_OBJ);
    DEF_ARG(a2, 2, VAR_OBJ);
    vmobj *newnode = a1->o;
    vmobj *refnode = a2->o;

    /* append to the parent */
    KL_SET_PROPERTY_O(newnode, parentNode, parent->o);
    vmobj *po = parent->o;
    if (!array_insert_after_obj(ctx, po, refnode, newnode)) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_XML_ERROR, "Reference node not found");
    }
    vmvar *children = hashmap_search(po, "children");
    if (children && children->t == VAR_OBJ) {
        children->o->idxsz = 0; // clear nodes.
        for (int i = 0, n = children->o->idxsz; i < n; i++) {
            array_push(ctx, children->o, po->ary[i]);
        }
    }
    if (po->idxsz > 0) {
        KL_SET_PROPERTY(po, firstChild, po->ary[0]);
        KL_SET_PROPERTY(po, lastNode, po->ary[po->idxsz - 1]);
    }

    /* adjustment of siblings */
    vmvar *next = hashmap_search(refnode, "nextSibling");
    if (next && next->t == VAR_OBJ) {
        KL_SET_PROPERTY_O(next->o, previousSibling, newnode);
        KL_SET_PROPERTY(newnode, nextSibling, next);
    }
    KL_SET_PROPERTY_O(refnode, nextSibling, newnode);
    KL_SET_PROPERTY_O(newnode, previousSibling, refnode);

    return 0;
}

static int XmlDom_appendChild(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(parent, 0, VAR_OBJ);
    DEF_ARG(a1, 1, VAR_OBJ);
    vmobj *newnode = a1->o;

    /* remove from the parent */
    KL_SET_PROPERTY_O(newnode, parentNode, parent->o);
    vmobj *po = parent->o;
    vmvar *last = po->ary[po->idxsz - 1];
    vmvar *nobj = alcvar_obj(ctx, newnode);
    array_push(ctx, po, nobj);
    vmvar *children = hashmap_search(po, "children");
    if (children && children->t == VAR_OBJ) {
        array_push(ctx, children->o, nobj);
    }

    if (po->idxsz == 1) {
        KL_SET_PROPERTY_O(po, firstChild, newnode);
    }
    KL_SET_PROPERTY_O(po, lastNode, newnode);

    /* adjustment of siblings */
    if (last && last->t == VAR_OBJ) {
        KL_SET_PROPERTY_O(last->o, nextSibling, newnode);
        KL_SET_PROPERTY(newnode, previousSibling, last);
    }

    return 0;
}

static int XmlDom_removeChild(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(parent, 0, VAR_OBJ);
    DEF_ARG(a1, 1, VAR_OBJ);
    vmobj *node = a1->o;

    XmlDom_removeNode(ctx, parent, node);

    SET_I64(r, 0);
    return 0;
}

static int XmlDom_replaceNodeFromParent(vmctx *ctx, vmobj *po, vmobj *node1, vmobj *node2)
{
    int pos = array_replace_obj(ctx, po, node1, node2);
    if (pos < 0) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_XML_ERROR, "Node to replace not found");
    }
    vmvar *children = hashmap_search(po, "children");
    if (children && children->t == VAR_OBJ && pos < children->o->idxsz) {
        children->o->ary[pos] = po->ary[pos];
    }
    if (pos == 0) {
        KL_SET_PROPERTY_O(po, firstChild, node2);
    } else if (pos == po->idxsz - 1) {
        KL_SET_PROPERTY_O(po, lastNode, node2);
    }

    KL_SET_PROPERTY_O(node2, parentNode, po);
    vmvar *next = hashmap_search(node1, "nextSibling");
    if (next) {
        KL_SET_PROPERTY(node2, nextSibling, next);
        KL_SET_PROPERTY_O(next->o, previousSibling, node2);
    }
    vmvar *prev = hashmap_search(node1, "previousSibling");
    if (prev) {
        KL_SET_PROPERTY(node2, previousSibling, prev);
        KL_SET_PROPERTY_O(prev->o, nextSibling, node2);
    }

    /* remove node1 from tree, but child nodes will still be held. */
    hashmap_remove(ctx, node1, "parentNode");
    hashmap_remove(ctx, node1, "previousSibling");
    hashmap_remove(ctx, node1, "nextSibling");

    return 0;
}

static int XmlDom_replaceNode(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(node1, 0, VAR_OBJ);
    DEF_ARG(node2, 1, VAR_OBJ);
    vmvar *parent = hashmap_search(node1->o, "parentNode");
    if (!parent || parent->t != VAR_OBJ) {
        return throw_system_exception(__LINE__, ctx, EXCEPT_XML_ERROR, "Invalid XML node");
    }

    int e = XmlDom_replaceNodeFromParent(ctx, parent->o, node1->o, node2->o);
    if (e != 0) {
        return e;
    }

    SET_I64(r, 0);
    return 0;
}

static int XmlDom_replaceChild(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(parent, 0, VAR_OBJ);
    DEF_ARG(a1, 1, VAR_OBJ);
    DEF_ARG(a2, 2, VAR_OBJ);
    vmobj *node1 = a1->o;
    vmobj *node2 = a2->o;

    int e = XmlDom_replaceNodeFromParent(ctx, parent->o, node1, node2);
    if (e != 0) {
        return e;
    }

    SET_I64(r, 0);
    return 0;
}

static void XmlDom_getTextContent(vmctx *ctx, vmstr *text, vmvar *node)
{
    int node_type = hashmap_getint(node->o, "nodeType", -1);
    if (node_type == XMLDOM_TEXT_NODE || node_type == XMLDOM_CDATA_SECTION_NODE ||
            node_type == XMLDOM_COMMENT_NODE || node_type == XMLDOM_PROCESSING_INSTRUCTION_NODE) {
        const char *node_value = hashmap_getstr(node->o, "nodeValue");
        str_append_cp(ctx, text, node_value);
        return;
    }

    vmobj *c = node->o;
    for (int i = 0, n = c->idxsz; i < n; i++) {
        XmlDom_getTextContent(ctx, text, c->ary[i]);
    }
}

static int XmlDom_textContent(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(node, 0, VAR_OBJ);
    vmstr *text = alcstr_str(ctx, "");
    XmlDom_getTextContent(ctx, text, node);
    SET_SV(r, text);
    return 0;
}

static void setup_xmlnode_props(vmctx *ctx, vmfrm *lex, vmvar *doc, vmvar *r, vmvar *parent, int index, int type, xmlctx *xctx)
{
    vmobj *o = r->o;
    o->is_sysobj = 1;
    KL_SET_PROPERTY_I(o, _nodeid, xctx->id++);
    KL_SET_PROPERTY_I(o, nodeType, type);
    KL_SET_PROPERTY_I(o, hasChildNodes, o->idxsz > 0 ? 1 : 0);
    vmobj *children = alcobj(ctx);
    for (int i = 0, n = o->idxsz; i < n; i++) {
        array_push(ctx, children, o->ary[i]);
    }
    KL_SET_PROPERTY(o, children, alcvar_obj(ctx, children));
    KL_SET_PROPERTY(o, ownerDocument, doc);
    KL_SET_PROPERTY(o, parentNode, parent);
    if (o->idxsz > 0) {
        KL_SET_PROPERTY(o, firstChild, o->ary[0]);
        KL_SET_PROPERTY(o, lastChild, o->ary[o->idxsz - 1]);
    } else {
        KL_SET_PROPERTY(o, firstChild, NULL);
        KL_SET_PROPERTY(o, lastChild, NULL);
    }
    if (index > 0) {
        vmobj *p = parent->o;
        vmvar *prev = p->ary[index - 1];
        KL_SET_PROPERTY(o, previousSibling, prev);
        KL_SET_PROPERTY(prev->o, nextSibling, r);
    }

    KL_SET_METHOD(o, getElementById, XmlDom_getElementById, lex, 2);
    KL_SET_METHOD(o, getElementsByTagName, XmlDom_getElementsByTagName, lex, 2);
    KL_SET_METHOD(o, remove, XmlDom_remove, lex, 1);
    KL_SET_METHOD(o, insertBefore, XmlDom_insertBefore, lex, 3);
    KL_SET_METHOD(o, insertAfter, XmlDom_insertAfter, lex, 3);
    KL_SET_METHOD(o, appendChild, XmlDom_appendChild, lex, 2);
    KL_SET_METHOD(o, removeChild, XmlDom_removeChild, lex, 2);
    KL_SET_METHOD(o, replaceChild, XmlDom_replaceChild, lex, 3);
    KL_SET_METHOD(o, replaceNode, XmlDom_replaceNode, lex, 3);
    KL_SET_METHOD(o, textContent, XmlDom_textContent, lex, 3);
    KL_SET_METHOD(o, innerText, XmlDom_textContent, lex, 3);
    KL_SET_METHOD(o, xpath, XPath_evaluate, lex, 2);
}

static void setup_xmldoc_props(vmctx *ctx, vmfrm *lex, vmvar *r)
{
    vmobj *o = r->o;
    vmvar **ary = o->ary;
    int n = o->idxsz;
    for (int i = 0; i < n; i++) {
        vmobj *vo = ary[i]->o;
        vmvar *v = hashmap_search(vo, "nodeType");
        if (v && v->t == VAR_INT64) {
            if (v->i == XMLDOM_ELEMENT_NODE) {
                KL_SET_PROPERTY(o, documentElement, ary[i]);
            }
        }
    }
    KL_SET_PROPERTY_S(o, nodeName, "#document");
    KL_SET_METHOD(o, xpath, XPath_evaluate, lex, 2);
}

static const char *parse_doc(vmctx *ctx, vmfrm *lex, vmvar *doc, vmobj *nsmap, vmvar *r, const char *p, xmlctx *xctx)
{
    while (*p != 0) {
        const char *s = p;
        while (*p != '<') {
            if (*p == 0) break;
            move_next(p);
        }
        if (s != p) {
            vmvar *vs = make_str_obj(ctx, &(xctx->err), s, p - s);
            if (xctx->err < 0) return p;
            if (xctx->normalize_space) {
                str_trim(ctx, vs->s, " \t\r\n");
            }
            if (!xctx->normalize_space || vs->s->len > 0) {
                vmvar *vo = alcvar_obj(ctx, alcobj(ctx));
                KL_SET_PROPERTY(vo->o, text, vs);
                KL_SET_PROPERTY(vo->o, nodeValue, vs);
                KL_SET_PROPERTY_S(vo->o, nodeName, "#text");
                setup_xmlnode_props(ctx, lex, doc, vo, r, r->o->idxsz, XMLDOM_TEXT_NODE, xctx);
                array_push(ctx, r->o, vo);
            }
        }
        if (*p == 0) break;
        if (*p == '<') {
            vmobj *cnsmap = object_copy(ctx, nsmap);
            move_next(p);
            if (*p == '/') return p;
            vmvar *n = alcvar_obj(ctx, alcobj(ctx));
            if (*p == '!') {
                syntax_error((xctx->err), (*++p != '-'));
                syntax_error((xctx->err), (*++p != '-'));
                p = parse_comment(ctx, n, p+1, xctx);
                setup_xmlnode_props(ctx, lex, doc, n, r, r->o->idxsz, XMLDOM_COMMENT_NODE, xctx);
            } else if (*p == '?') {
                move_next(p);
                int xmldecl = 0;
                p = parse_pi(ctx, cnsmap, r, n, p, xctx, &xmldecl);
                if (xmldecl && (xctx->depth > 0 || r->o->idxsz > 0)) {
                    errset_wuth_ret((xctx->err), XMLDOM_EDECL);
                }
                setup_xmlnode_props(ctx, lex, doc, n, r, r->o->idxsz,
                    xmldecl ? XMLDOM_XMLDECL_NODE : XMLDOM_PROCESSING_INSTRUCTION_NODE, xctx);
            } else {
                p = parse_node(ctx, lex, doc, cnsmap, n, p, xctx);
                setup_xmlnode_props(ctx, lex, doc, n, r, r->o->idxsz, XMLDOM_ELEMENT_NODE, xctx);
            }
            if (xctx->err < 0) return p;
            array_push(ctx, r->o, n);
        }
        if (*p == 0) break;
        syntax_error((xctx->err), (*p != '>'));
        move_next(p);
    }
    return p;
}

static void check_options(vmobj *opts, xmlctx *xctx)
{
    vmvar *normalizeSpace = hashmap_search(opts, "normalizeSpace");
    if (normalizeSpace && (normalizeSpace->t == VAR_INT64 || normalizeSpace->t == VAR_BOOL)) {
        xctx->normalize_space = normalizeSpace->i;
    }
}

static int XmlDom_parseString(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    DEF_ARG_OR_UNDEF(a1, 1, VAR_OBJ);
    vmstr *s = a0->s;
    const char *str = s->hd;
    while (is_whitespace(*str)) {
        ++str;
    }
    vmobj *nsmap = alcobj(ctx);
    r->t = VAR_OBJ;
    r->o = alcobj(ctx);
    r->o->is_sysobj = 1;
    if (s->s) {
        xmlctx xctx = {
            .err = 0,
            .line = 1,
            .pos = 0,
            .depth = 0,
            .id = 1,
            .normalize_space = 1,
        };
        if (a1->t == VAR_OBJ) {
            check_options(a1->o, &xctx);
        }
        parse_doc(ctx, lex, r, nsmap, r, str, &xctx);
        setup_xmldoc_props(ctx, lex, r);
        if (xctx.err < 0) {
            return xmldom_error(ctx, xctx.err, xctx.line, xctx.pos);
        }
        mark_and_sweep(ctx);
    }
    return 0;
}

int XmlDom(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, parseString, XmlDom_parseString, lex, 1);
    KL_SET_PROPERTY_I(o, XMLDECL_NODE,                              XMLDOM_XMLDECL_NODE);
    KL_SET_PROPERTY_I(o, ATTRIBUTE_NODE,                            XMLDOM_ATTRIBUTE_NODE);
    KL_SET_PROPERTY_I(o, CDATA_SECTION_NODE,                        XMLDOM_CDATA_SECTION_NODE);
    KL_SET_PROPERTY_I(o, COMMENT_NODE,                              XMLDOM_COMMENT_NODE);
    KL_SET_PROPERTY_I(o, DOCUMENT_FRAGMENT_NODE,                    XMLDOM_DOCUMENT_FRAGMENT_NODE);
    KL_SET_PROPERTY_I(o, DOCUMENT_NODE,                             XMLDOM_DOCUMENT_NODE);
    KL_SET_PROPERTY_I(o, DOCUMENT_POSITION_CONTAINED_BY,            XMLDOM_DOCUMENT_POSITION_CONTAINED_BY);
    KL_SET_PROPERTY_I(o, DOCUMENT_POSITION_CONTAINS,                XMLDOM_DOCUMENT_POSITION_CONTAINS);
    KL_SET_PROPERTY_I(o, DOCUMENT_POSITION_DISCONNECTED,            XMLDOM_DOCUMENT_POSITION_DISCONNECTED);
    KL_SET_PROPERTY_I(o, DOCUMENT_POSITION_FOLLOWING,               XMLDOM_DOCUMENT_POSITION_FOLLOWING);
    KL_SET_PROPERTY_I(o, DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC, XMLDOM_DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);
    KL_SET_PROPERTY_I(o, DOCUMENT_POSITION_PRECEDING,               XMLDOM_DOCUMENT_POSITION_PRECEDING);
    KL_SET_PROPERTY_I(o, DOCUMENT_TYPE_NODE,                        XMLDOM_DOCUMENT_TYPE_NODE);
    KL_SET_PROPERTY_I(o, ELEMENT_NODE,                              XMLDOM_ELEMENT_NODE);
    KL_SET_PROPERTY_I(o, ENTITY_NODE,                               XMLDOM_ENTITY_NODE);
    KL_SET_PROPERTY_I(o, ENTITY_REFERENCE_NODE,                     XMLDOM_ENTITY_REFERENCE_NODE);
    KL_SET_PROPERTY_I(o, NOTATION_NODE,                             XMLDOM_NOTATION_NODE);
    KL_SET_PROPERTY_I(o, PROCESSING_INSTRUCTION_NODE,               XMLDOM_PROCESSING_INSTRUCTION_NODE);
    KL_SET_PROPERTY_I(o, TEXT_NODE,                                 XMLDOM_TEXT_NODE);
    SET_OBJ(r, o);
    return 0;
}

/* XPath */

enum xpath_axis {
    XPATH_AXIS_UNKNOWN = 0,
    XPATH_AXIS_ANCESTOR,
    XPATH_AXIS_ANCESTOR_OR_SELF,
    XPATH_AXIS_ATTRIBUTE,
    XPATH_AXIS_CHILD,
    XPATH_AXIS_DESCENDANT,
    XPATH_AXIS_DESCENDANT_OR_SELF,
    XPATH_AXIS_FOLLOWING,
    XPATH_AXIS_FOLLOWING_SIBLING,
    XPATH_AXIS_NAMESPACE,
    XPATH_AXIS_PARENT,
    XPATH_AXIS_PRECEDING,
    XPATH_AXIS_PRECEDING_SIBLING,
    XPATH_AXIS_SELF,
    XPATH_AXIS_ROOT,
};

enum xpath_node_type {
    XPATH_NODE_ROOT,
    XPATH_NODE_ELEMENT,
    XPATH_NODE_ATTRIBUTE,
    XPATH_NODE_NAMESPACE,
    XPATH_NODE_TEXT,
    XPATH_NODE_PROCESSIGN_INSTRUCTION,
    XPATH_NODE_COMMENT,
    XPATH_NODE_ALL,
};

enum xpath_type {
    XPATH_TK_UNKNOWN,       // Unknown lexeme
    XPATH_TK_LOR,           // Operator 'or'
    XPATH_TK_LAND,          // Operator 'and'
    XPATH_TK_EQ,            // Operator '='
    XPATH_TK_NE,            // Operator '!='
    XPATH_TK_LT,            // Operator '<'
    XPATH_TK_LE,            // Operator '<='
    XPATH_TK_GT,            // Operator '>'
    XPATH_TK_GE,            // Operator '>='
    XPATH_TK_ADD,           // Operator '+'
    XPATH_TK_SUB,           // Operator '-'
    XPATH_TK_MUL,           // Operator '*'
    XPATH_TK_DIV,           // Operator 'div'
    XPATH_TK_MOD,           // Operator 'mod'
    XPATH_TK_UMINUS,        // Unary Operator '-'
    XPATH_TK_UNION,         // Operator '|'
    XPATH_TK_LAST_OPERATOR = XPATH_TK_UNION,

    XPATH_TK_DOTDOT,        // '..'
    XPATH_TK_COLONCOLON,    // '::'
    XPATH_TK_SLASHSLASH,    // Operator '//'
    XPATH_TK_INT,           // Numeric Literal for integer
    XPATH_TK_NUM,           // Numeric Literal for double
    XPATH_TK_AXISNAME,      // AxisName

    XPATH_TK_NAME,          // NameTest, NodeType, FunctionName, AxisName
    XPATH_TK_STR,           // String Literal
    XPATH_TK_EOF,           // End of the expression

    XPATH_TK_LP,            // '('
    XPATH_TK_RP,            // ')'
    XPATH_TK_LB,            // '['
    XPATH_TK_RB,            // ']'
    XPATH_TK_DOT,           // '.'
    XPATH_TK_AT,            // '@'
    XPATH_TK_COMMA,         // ','

    XPATH_TK_ASTA,          // '*' ... NameTest
    XPATH_TK_SLASH,         // '/' ... Operator '/'
};

const char *xpath_token_name[] = {
    "XPATH_TK_UNKNOWN",
    "XPATH_TK_LOR",
    "XPATH_TK_LAND",
    "XPATH_TK_EQ",
    "XPATH_TK_NE",
    "XPATH_TK_LT",
    "XPATH_TK_LE",
    "XPATH_TK_GT",
    "XPATH_TK_GE",
    "XPATH_TK_ADD",
    "XPATH_TK_SUB",
    "XPATH_TK_MUL",
    "XPATH_TK_DIV",
    "XPATH_TK_MOD",
    "XPATH_TK_UMINUS",
    "XPATH_TK_UNION",

    "XPATH_TK_DOTDOT",
    "XPATH_TK_COLONCOLON",
    "XPATH_TK_SLASHSLASH",
    "XPATH_TK_INT",
    "XPATH_TK_NUM",
    "XPATH_TK_AXISNAME",

    "XPATH_TK_NAME",
    "XPATH_TK_STR",
    "XPATH_TK_EOF",

    "XPATH_TK_LP",
    "XPATH_TK_RP",
    "XPATH_TK_LB",
    "XPATH_TK_RB",
    "XPATH_TK_DOT",
    "XPATH_TK_AT",
    "XPATH_TK_COMMA",

    "XPATH_TK_ASTA",
    "XPATH_TK_SLASH",
};

enum {
    XPATH_OP_UNKNOWN,
    XPATH_OP_LOR,
    XPATH_OP_LAND,
    XPATH_OP_EQ,
    XPATH_OP_NE,
    XPATH_OP_LT,
    XPATH_OP_LE,
    XPATH_OP_GT,
    XPATH_OP_GE,
    XPATH_OP_ADD,
    XPATH_OP_SUB,
    XPATH_OP_MUL,
    XPATH_OP_DIV,
    XPATH_OP_MOD,
    XPATH_OP_UMINUS,
    XPATH_OP_UNION,

    XPATH_OP_STEP,
    XPATH_OP_AXIS,
    XPATH_OP_PREDICATE,
    XPATH_OP_EXPR,
    XPATH_OP_FUNCCALL,
    XPATH_OP_BOOL,
    XPATH_OP_INT,
    XPATH_OP_NUM,
    XPATH_OP_STR,
};

typedef struct xpath_lexer {
    const char *expr;
    int exprlen;
    int curidx;
    char cur;
    int axis;
    int token;
    int prevtype;
    vmstr *name;
    vmstr *prefix;
    vmstr *str;
    vmstr *num;
    int function;
    int start;
    int prevend;
    int e;
    int stksz;
    int posinfo[256];
} xpath_lexer;

static vmvar *xpath_union_expr(vmctx *ctx, xpath_lexer *l);

#define xpath_is_bool(v) ((v)->t == VAR_BOOL)
#define xpath_is_number(v) ((v)->t == VAR_INT64 || (v)->t == VAR_DBL)
#define is_ascii_digit(x) ((unsigned int)((x) - '0') <= 9)
#define is_ncname_char(x) (('a' <= (x) && (x) <= 'z') || ('Z' <= (x) && (x) <= 'Z') || ('0' <= (x) && (x) <= '9') || ((x) == '_') || ((x) == '-'))
#define XPATH_CHECK_OBJ(v) do { if ((v)->t != VAR_OBJ) return xpath_set_invalid_state_exception(__LINE__, ctx); } while (0)
#define XPATH_CHECK_ERR(e) do { if (e != 0) return e; } while (0)
#define XPATH_EXPAND_V(v) do { if ((v)->t == VAR_OBJ && (v)->o->idxsz > 0) SHCOPY_VAR_TO(ctx, (v), (v)->o->ary[0]); } while (0)
#define XPATH_EXPAND_O(obj) do { if ((obj) && (obj)->idxsz > 0 && (obj)->ary[0]->t == VAR_OBJ) (obj) = (obj)->ary[0]->o; } while (0)

static void next_char(xpath_lexer *l)
{
    l->curidx++;
    if (l->curidx < l->exprlen) {
        l->cur = l->expr[l->curidx];
    } else {
        l->cur = 0;
    }
}

static void skip_xpah_whitespace(xpath_lexer *l)
{
    while (is_whitespace(l->cur)) {
        next_char(l);
    }
}

static void set_index(xpath_lexer *l, int idx)
{
    l->curidx = idx - 1;
    next_char(l);
}

static int check_axis(xpath_lexer *l)
{
    l->token = XPATH_TK_AXISNAME;

    if      (strcmp(l->name->hd, "ancestor")           == 0) return XPATH_AXIS_ANCESTOR;
    else if (strcmp(l->name->hd, "ancestor-or-self")   == 0) return XPATH_AXIS_ANCESTOR_OR_SELF;
    else if (strcmp(l->name->hd, "attribute")          == 0) return XPATH_AXIS_ATTRIBUTE;
    else if (strcmp(l->name->hd, "child")              == 0) return XPATH_AXIS_CHILD;
    else if (strcmp(l->name->hd, "descendant")         == 0) return XPATH_AXIS_DESCENDANT;
    else if (strcmp(l->name->hd, "descendant-or-self") == 0) return XPATH_AXIS_DESCENDANT_OR_SELF;
    else if (strcmp(l->name->hd, "following")          == 0) return XPATH_AXIS_FOLLOWING;
    else if (strcmp(l->name->hd, "following-sibling")  == 0) return XPATH_AXIS_FOLLOWING_SIBLING;
    else if (strcmp(l->name->hd, "namespace")          == 0) return XPATH_AXIS_NAMESPACE;
    else if (strcmp(l->name->hd, "parent")             == 0) return XPATH_AXIS_PARENT;
    else if (strcmp(l->name->hd, "preceding")          == 0) return XPATH_AXIS_PRECEDING;
    else if (strcmp(l->name->hd, "preceding-sibling")  == 0) return XPATH_AXIS_PRECEDING_SIBLING;
    else if (strcmp(l->name->hd, "self")               == 0) return XPATH_AXIS_SELF;

    l->token = XPATH_TK_NAME;
    return XPATH_AXIS_UNKNOWN;
}

static int check_operator(xpath_lexer *l, int asta)
{
    int optype;

    if (asta) {
        optype = XPATH_TK_MUL;
    } else {
        if ((strlen(l->prefix->hd) != 0) || (strlen(l->name->hd) > 3)) {
            return 0;
        }
        if      (strcmp(l->name->hd, "or")  == 0) optype = XPATH_TK_LOR;
        else if (strcmp(l->name->hd, "and") == 0) optype = XPATH_TK_LAND;
        else if (strcmp(l->name->hd, "div") == 0) optype = XPATH_TK_DIV;
        else if (strcmp(l->name->hd, "mod") == 0) optype = XPATH_TK_MOD;
        else return 0;
    }
    if (l->prevtype <= XPATH_TK_LAST_OPERATOR) {
        return 0;
    }
    switch (l->prevtype) {
    case XPATH_TK_SLASH:
    case XPATH_TK_SLASHSLASH:
    case XPATH_TK_AT:
    case XPATH_TK_COLONCOLON:
    case XPATH_TK_LP:
    case XPATH_TK_LB:
    case XPATH_TK_COMMA:
        return 0;
    }

    l->token = optype;
    return 1;
}

static int get_string(vmctx *ctx, xpath_lexer *l)
{
    int startidx = l->curidx + 1;
    int endch = l->cur;
    next_char(l);
    while (l->cur != endch && l->cur != 0) {
        next_char(l);
    }

    if (l->cur == 0) {
        return 0;
    }

    str_set(ctx, l->str, l->expr + startidx, l->curidx - startidx);
    next_char(l);
    return 1;
}

static int get_ncname(vmctx *ctx, xpath_lexer *l, vmstr *name)
{
    int startidx = l->curidx;
    while (is_ncname_char(l->cur)) {
        next_char(l);
    }

    str_set(ctx, name, l->expr + startidx, l->curidx - startidx);
    return 1;
}

static int get_number(vmctx *ctx, xpath_lexer *l)
{
    int r = XPATH_TK_INT;
    int startidx = l->curidx;
    while (is_ascii_digit(l->cur)) {
        next_char(l);
    }
    if (l->cur == '.') {
        r = XPATH_TK_NUM;
        next_char(l);
        while (is_ascii_digit(l->cur)) {
            next_char(l);
        }
    }
    if ((l->cur & (~0x20)) == 'E') {
        return 0;
    }

    str_set(ctx, l->num, l->expr + startidx, l->curidx - startidx);
    return r;
}

static void next_token(vmctx *ctx, xpath_lexer *l)
{
    l->prevend = l->cur;
    l->prevtype = l->token;
    skip_xpah_whitespace(l);
    l->start = l->curidx;

    switch (l->cur) {
    case 0:   l->token = XPATH_TK_EOF; return;
    case '(': l->token = XPATH_TK_LP;     next_char(l); break;
    case ')': l->token = XPATH_TK_RP;     next_char(l); break;
    case '[': l->token = XPATH_TK_LB;     next_char(l); break;
    case ']': l->token = XPATH_TK_RB;     next_char(l); break;
    case '@': l->token = XPATH_TK_AT;     next_char(l); break;
    case ',': l->token = XPATH_TK_COMMA;  next_char(l); break;

    case '.':
        next_char(l);
        if (l->cur == '.') {
            l->token = XPATH_TK_DOTDOT;
            next_char(l);
        } else if (is_ascii_digit(l->cur)) {
            l->curidx = l->start - 1;
            next_char(l);
            goto CASE_0;
        } else {
            l->token = XPATH_TK_DOT;
        }
        break;
    case ':':
        next_char(l);
        if (l->cur == ':') {
            l->token = XPATH_TK_COLONCOLON;
            next_char(l);
        } else {
            l->token = XPATH_TK_UNKNOWN;
        }
        break;
    case '*':
        l->token = XPATH_TK_ASTA;
        next_char(l);
        check_operator(l, 1);
        break;
    case '/':
        next_char(l);
        if (l->cur == '/') {
            l->token = XPATH_TK_SLASHSLASH;
            next_char(l);
        } else {
            l->token = XPATH_TK_SLASH;
        }
        break;
    case '|':
        l->token = XPATH_TK_UNION;
        next_char(l);
        break;
    case '+':
        l->token = XPATH_TK_ADD;
        next_char(l);
        break;
    case '-':
        l->token = XPATH_TK_SUB;
        next_char(l);
        break;
    case '=':
        l->token = XPATH_TK_EQ;
        next_char(l);
        break;
    case '!':
        next_char(l);
        if (l->cur == '=') {
            l->token = XPATH_TK_NE;
            next_char(l);
        } else {
            l->token = XPATH_TK_UNKNOWN;
        }
        break;
    case '<':
        next_char(l);
        if (l->cur == '=') {
            l->token = XPATH_TK_LE;
            next_char(l);
        } else {
            l->token = XPATH_TK_LT;
        }
        break;
    case '>':
        next_char(l);
        if (l->cur == '=') {
            l->token = XPATH_TK_GE;
            next_char(l);
        } else {
            l->token = XPATH_TK_GT;
        }
        break;
    case '"':
    case '\'':
        l->token = XPATH_TK_STR;
        if (!get_string(ctx, l)) {
            return;
        }
        break;
    case '0':
CASE_0:;
    case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
        l->token = get_number(ctx, l);
        if (!l->token) {
            return;
        }
        break;
    default:
        get_ncname(ctx, l, l->name);
        if (l->name->len > 0) {
            l->token = XPATH_TK_NAME;
            str_clear(l->prefix);
            l->function = 0;
            l->axis = XPATH_AXIS_UNKNOWN;
            int colon2 = 0;
            int savedidx = l->curidx;
            if (l->cur == ':') {
                next_char(l);
                if (l->cur == ':') {
                    next_char(l);
                    colon2 = 1;
                    set_index(l, savedidx);
                } else {
                    vmstr ncname;
                    get_ncname(ctx, l, &ncname);
                    if (ncname.len > 0) {
                        str_append_cp(ctx, l->prefix, l->name->hd);
                        str_set_cp(ctx, l->name, ncname.hd);
                        savedidx = l->curidx;
                        skip_xpah_whitespace(l);
                        l->function = (l->cur == '(');
                        set_index(l, savedidx);
                    } else if (l->cur == '*') {
                        next_char(l);
                        str_append_cp(ctx, l->prefix, l->name->hd);
                        str_set_cp(ctx, l->name, "*");
                    } else {
                        set_index(l, savedidx);
                    }
                }
            } else {
                skip_xpah_whitespace(l);
                if (l->cur == ':') {
                    next_char(l);
                    if (l->cur == ':') {
                        next_char(l);
                        colon2 = 1;
                    }
                    set_index(l, savedidx);
                } else {
                    l->function = (l->cur == '(');
                }
            }
            if (!check_operator(l, 0) && colon2) {
                l->axis = check_axis(l);
            }
        } else {
            l->token = XPATH_TK_UNKNOWN;
            next_char(l);
        }
        break;
    }
}

/*
 * UnionExpr ::= PathExpr ('|' PathExpr)*
 * PathExpr ::= LocationPath | FilterExpr (('/' | '//') RelativeLocationPath )?
 * FilterExpr ::= PrimaryExpr Predicate*
 * PrimaryExpr ::= Literal | Number | '(' Expr ')' | FunctionCall
 * FunctionCall ::= FunctionName '(' (Expr (',' Expr)* )? ')'
 *
 * LocationPath ::= RelativeLocationPath | '/' RelativeLocationPath? | '//' RelativeLocationPath
 * RelativeLocationPath ::= Step (('/' | '//') Step)*
 * Step ::= '.' | '..' | (AxisName '::' | '@')? NodeTest Predicate*
 * NodeTest ::= NameTest | ('comment' | 'text' | 'node') '(' ')' | 'processing-instruction' '('  Literal? ')'
 * NameTest ::= '*' | NCName ':' '*' | QName
 * Predicate ::= '[' Expr ']'
 *
 * Expr   ::= OrExpr
 * OrExpr ::= AndExpr ('or' AndExpr)*
 * AndExpr ::= EqualityExpr ('and' EqualityExpr)*
 * EqualityExpr ::= RelationalExpr (('=' | '!=') RelationalExpr)*
 * RelationalExpr ::= AdditiveExpr (('<' | '>' | '<=' | '>=') AdditiveExpr)*
 * AdditiveExpr ::= MultiplicativeExpr (('+' | '-') MultiplicativeExpr)*
 * MultiplicativeExpr ::= UnaryExpr (('*' | 'div' | 'mod') UnaryExpr)*
 * UnaryExpr ::= ('-')* UnionExpr
 */

static int xpath_operator_priority[] = {
    /* XPATH_TK_UNKNOWN */ 0,
    /* XPATH_TK_LOR     */ 1,
    /* XPATH_TK_LAND    */ 2,
    /* XPATH_TK_EQ      */ 3,
    /* XPATH_TK_NE      */ 3,
    /* XPATH_TK_LT      */ 4,
    /* XPATH_TK_LE      */ 4,
    /* XPATH_TK_GT      */ 4,
    /* XPATH_TK_GE      */ 4,
    /* XPATH_TK_ADD     */ 5,
    /* XPATH_TK_SUB     */ 5,
    /* XPATH_TK_MUL     */ 6,
    /* XPATH_TK_DIV     */ 6,
    /* XPATH_TK_MOD     */ 6,
    /* XPATH_TK_UMINUS  */ 7,
    /* XPATH_TK_UNION   */ 8,
};

static void xpath_set_error(int line, vmctx *ctx, xpath_lexer *l, const char *msg)
{
    printf("XPath error at %d\n", line);
    l->e = throw_system_exception(__LINE__, ctx, EXCEPT_XML_ERROR, msg);
}

static void xpath_set_syntax_error(int line, vmctx *ctx, xpath_lexer *l)
{
    xpath_set_error(line, ctx, l, "XPath syntax error");
}

static int xpath_set_exception(int line, vmctx *ctx, xpath_lexer *l, const char *msg)
{
    printf("XPath error at %d\n", line);
    return throw_system_exception(__LINE__, ctx, EXCEPT_XML_ERROR, msg);
}

static int xpath_set_invalid_state_exception(int line, vmctx *ctx)
{
    printf("XPath error at %d\n", line);
    return throw_system_exception(__LINE__, ctx, EXCEPT_XML_ERROR, "Invalid XPath state");
}

static vmvar *xpath_make_predicate(vmctx *ctx, vmvar *node, vmvar *condition, int reverse)
{
    vmobj *n = alcobj(ctx);
    KL_SET_PROPERTY_I(n, op, XPATH_OP_PREDICATE);
    KL_SET_PROPERTY_I(n, isPredicate, 1);
    array_push(ctx, n, node);
    array_push(ctx, n, condition);
    array_push(ctx, n, alcvar_int64(ctx, reverse, 0));
    return alcvar_obj(ctx, n);
}

static vmvar *xpath_make_axis(vmctx *ctx, int axis, int nodetype, vmvar *prefix, vmvar *name)
{
    vmobj *n = alcobj(ctx);
    KL_SET_PROPERTY_I(n, op, XPATH_OP_AXIS);
    KL_SET_PROPERTY_I(n, isAxis, 1);
    array_push(ctx, n, alcvar_int64(ctx, axis, 0));
    array_push(ctx, n, alcvar_int64(ctx, nodetype, 0));
    array_push(ctx, n, prefix);
    array_push(ctx, n, name);
    return alcvar_obj(ctx, n);
}

static vmvar *xpath_make_function(vmctx *ctx, vmvar *prefix, vmvar *name, vmobj *args)
{
    vmobj *n = alcobj(ctx);
    KL_SET_PROPERTY_I(n, op, XPATH_OP_FUNCCALL);
    KL_SET_PROPERTY_I(n, isFunctionCall, 1);
    array_push(ctx, n, prefix);
    array_push(ctx, n, name);
    array_push(ctx, n, args ? alcvar_obj(ctx, args) : NULL);
    return alcvar_obj(ctx, n);
}

static vmvar *xpath_make_binop(vmctx *ctx, int op, vmvar *lhs, vmvar *rhs)
{
    vmobj *n = alcobj(ctx);
    KL_SET_PROPERTY_I(n, op, XPATH_OP_EXPR);
    KL_SET_PROPERTY_I(n, isExpr, 1);
    array_push(ctx, n, alcvar_int64(ctx, op, 0));
    array_push(ctx, n, lhs);
    array_push(ctx, n, rhs);
    return alcvar_obj(ctx, n);
}

static vmvar *xpath_make_step(vmctx *ctx, vmvar *lhs, vmvar *rhs)
{
    vmobj *n = alcobj(ctx);
    KL_SET_PROPERTY_I(n, op, XPATH_OP_STEP);
    KL_SET_PROPERTY_I(n, isStep, 1);
    array_push(ctx, n, lhs);
    array_push(ctx, n, rhs);
    return alcvar_obj(ctx, n);
}

static int is_node_type(xpath_lexer *l)
{
    return l->prefix->len == 0 && (
        strcmp(l->name->hd, "node") == 0 || strcmp(l->name->hd, "text") == 0 ||
        strcmp(l->name->hd, "processing-instruction") == 0 || strcmp(l->name->hd, "comment") == 0
    );
}

static int is_step(int token)
{
    return
        token == XPATH_TK_DOT      ||
        token == XPATH_TK_DOTDOT   ||
        token == XPATH_TK_AT       ||
        token == XPATH_TK_AXISNAME ||
        token == XPATH_TK_ASTA     ||
        token == XPATH_TK_NAME
    ;
}

static int is_primary_expr(xpath_lexer *l)
{
    return
        l->token == XPATH_TK_STR ||
        l->token == XPATH_TK_INT ||
        l->token == XPATH_TK_NUM ||
        l->token == XPATH_TK_LP ||
        (l->token == XPATH_TK_NAME && l->function && !is_node_type(l))
    ;
}

static int is_reverse_axis(int axis)
{
    return
        axis == XPATH_AXIS_ANCESTOR         || axis == XPATH_AXIS_PRECEDING ||
        axis == XPATH_AXIS_ANCESTOR_OR_SELF || axis == XPATH_AXIS_PRECEDING_SIBLING
    ;
}

static int xpath_make_node_type(int axis)
{
    return axis == XPATH_AXIS_ATTRIBUTE ? XPATH_NODE_ATTRIBUTE :
        (axis == XPATH_AXIS_NAMESPACE ? XPATH_NODE_NAMESPACE : XPATH_NODE_ELEMENT);
}

static void push_posinfo(xpath_lexer *l, int startch, int endch)
{
    l->posinfo[l->stksz++] = startch;
    l->posinfo[l->stksz++] = endch;
}

static void pop_posinfo(xpath_lexer *l)
{
    l->stksz -= 2;
}

static int xpath_check_node_test(vmctx *ctx, xpath_lexer *l, int axis, int *node_type, vmstr *node_prefix, vmstr *node_name)
{
    switch (l->token) {
    case XPATH_TK_NAME:
        if (l->function && is_node_type(l)) {
            str_clear(node_prefix);
            str_clear(node_name);
            if      (strcmp(l->name->hd, "comment") == 0) *node_type = XPATH_NODE_COMMENT;
            else if (strcmp(l->name->hd, "text")    == 0) *node_type = XPATH_NODE_TEXT;
            else if (strcmp(l->name->hd, "node")    == 0) *node_type = XPATH_NODE_ALL;
            else                                          *node_type = XPATH_NODE_PROCESSIGN_INSTRUCTION;
            next_token(ctx, l);
            if (l->token != XPATH_TK_LP) {
                xpath_set_syntax_error(__LINE__, ctx, l);
                return 0;
            }
            next_token(ctx, l);

            if (*node_type == XPATH_NODE_PROCESSIGN_INSTRUCTION) {
                if (l->token != XPATH_TK_RP) {
                    if (l->token != XPATH_TK_STR) {
                        xpath_set_syntax_error(__LINE__, ctx, l);
                        return 0;
                    }
                    str_clear(node_prefix);
                    str_set_cp(ctx, node_name, l->str->hd);
                    next_token(ctx, l);
                }
            }

            if (l->token != XPATH_TK_RP) {
                xpath_set_syntax_error(__LINE__, ctx, l);
                return 0;
            }
            next_token(ctx, l);
        } else {
            str_set_cp(ctx, node_prefix, l->prefix->hd);
            str_set_cp(ctx, node_name, l->name->hd);
            *node_type = xpath_make_node_type(axis);
            next_token(ctx, l);
            if (strcmp(node_name->hd, "*") == 0) {
                str_clear(node_name);
            }
        }
        break;
    case XPATH_TK_ASTA:
        str_clear(node_prefix);
        str_clear(node_name);
        *node_type = xpath_make_node_type(axis);
        next_token(ctx, l);
        break;
    default :
        xpath_set_syntax_error(__LINE__, ctx, l);
    }
    return 1;
}

static vmvar *xpath_node_test(vmctx *ctx, xpath_lexer *l, int axis) {
    int node_type;
    vmstr *node_prefix = alcstr_str(ctx, "");
    vmstr *node_name = alcstr_str(ctx, "");

    vmvar *n = NULL;
    int startch = l->start;
    if (xpath_check_node_test(ctx, l, axis, &node_type, node_prefix, node_name)) {
        push_posinfo(l, startch, l->prevend);
        n = xpath_make_axis(ctx, axis, node_type, alcvar_str(ctx, node_prefix->hd), alcvar_str(ctx, node_name->hd));
        pop_posinfo(l);
    }

    pbakstr(ctx, node_name);
    pbakstr(ctx, node_prefix);
    return n;
}

static vmvar *xpath_process_expr(vmctx *ctx, xpath_lexer *l, int prec)
{
    int op = XPATH_TK_UNKNOWN;
    vmvar *n = NULL;

    // Unary operators
    if (l->token == XPATH_TK_SUB) {
        op = XPATH_TK_UMINUS;
        int opprec = xpath_operator_priority[op];
        next_token(ctx, l);
        vmvar *n2 =  xpath_process_expr(ctx, l, opprec);
        if (!n2) return NULL;
        n = xpath_make_binop(ctx, op, n2, NULL);
    } else {
        n = xpath_union_expr(ctx, l);
        if (!n) return NULL;
    }

    // Binary operators
    for ( ; ; ) {
        op = (l->token <= XPATH_TK_LAST_OPERATOR) ? l->token : XPATH_TK_UNKNOWN;
        int opprec = xpath_operator_priority[op];
        if (opprec <= prec) {
            break;
        }

        next_token(ctx, l);
        vmvar *n2 =  xpath_process_expr(ctx, l, opprec);
        if (!n2) return NULL;
        n = xpath_make_binop(ctx, op, n, n2);
    }

    return n;
}

static vmvar *xpath_expr(vmctx *ctx, xpath_lexer *l)
{
    return xpath_process_expr(ctx, l, 0);
}

static vmvar *xpath_predicate(vmctx *ctx, xpath_lexer *l)
{
    if (l->token != XPATH_TK_LB) {
        xpath_set_syntax_error(__LINE__, ctx, l);
        return NULL;
    }
    next_token(ctx, l);
    vmvar *n = xpath_expr(ctx, l);
    if (!n) return NULL;
    if (l->token != XPATH_TK_RB) {
        xpath_set_syntax_error(__LINE__, ctx, l);
        return NULL;
    }
    next_token(ctx, l);
    return n;
}

static vmvar *xpath_step(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = NULL;
    if (l->token == XPATH_TK_DOT) {
        next_token(ctx, l);
        n = xpath_make_axis(ctx, XPATH_AXIS_SELF, XPATH_NODE_ALL, NULL, NULL);
    } else if (l->token == XPATH_TK_DOTDOT) {
        next_token(ctx, l);
        n = xpath_make_axis(ctx, XPATH_AXIS_PARENT, XPATH_NODE_ALL, NULL, NULL);
    } else {
        int axis;
        switch (l->token) {
        case XPATH_TK_AXISNAME:
            axis = l->axis;
            next_token(ctx, l);
            next_token(ctx, l);
            break;
        case XPATH_TK_AT:
            axis = XPATH_AXIS_ATTRIBUTE;
            next_token(ctx, l);
            break;
        case XPATH_TK_NAME:
        case XPATH_TK_ASTA:
            axis = XPATH_AXIS_CHILD;
            break;
        default:
            xpath_set_syntax_error(__LINE__, ctx, l);
            return NULL;
        }
        n = xpath_node_test(ctx, l, axis);
        if (!n) return NULL;

        while (l->token == XPATH_TK_LB) {
            vmvar *n2 = xpath_predicate(ctx, l);
            if (!n2) return NULL;
            n = xpath_make_predicate(ctx, n, n2, is_reverse_axis(axis));
        }
    }

    return n;
}

static vmvar *xpath_relative_location_path(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = xpath_step(ctx, l);
    if (!n) return NULL;
    if (l->token == XPATH_TK_SLASH) {
        next_token(ctx, l);
        vmvar *n2 = xpath_relative_location_path(ctx, l);
        if (!n2) return NULL;
        n = xpath_make_step(ctx, n, n2);
    } else if (l->token == XPATH_TK_SLASHSLASH) {
        next_token(ctx, l);
        vmvar *n2 = xpath_relative_location_path(ctx, l);
        if (!n2) return NULL;
        n = xpath_make_step(ctx, n,
            xpath_make_step(ctx,
                xpath_make_axis(ctx, XPATH_AXIS_DESCENDANT_OR_SELF, XPATH_NODE_ALL, NULL, NULL), n2
            )
        );
    }
    return n;
}

static vmvar *xpath_location_path(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = NULL;
    if (l->token == XPATH_TK_SLASH) {
        next_token(ctx, l);
        n = xpath_make_axis(ctx, XPATH_AXIS_ROOT, XPATH_NODE_ALL, NULL, NULL);
        if (is_step(l->token)) {
            vmvar *n2 = xpath_relative_location_path(ctx, l);
            if (!n2) return NULL;
            n = xpath_make_step(ctx, n, n2);
        }
    } else if (l->token == XPATH_TK_SLASHSLASH) {
        next_token(ctx, l);
        vmvar *n2 = xpath_relative_location_path(ctx, l);
        if (!n2) return NULL;
        n = xpath_make_step(ctx,
            xpath_make_axis(ctx, XPATH_AXIS_ROOT, XPATH_NODE_ALL, NULL, NULL),
            xpath_make_step(ctx,
                xpath_make_axis(ctx, XPATH_AXIS_DESCENDANT_OR_SELF, XPATH_NODE_ALL, NULL, NULL), n2
            )
        );
    } else {
        n = xpath_relative_location_path(ctx, l);
        if (!n) return NULL;
    }
    return n;
}

static vmvar *xpath_function_call(vmctx *ctx, xpath_lexer *l)
{
    vmobj *args = alcobj(ctx);
    vmvar *name = alcvar_str(ctx, l->name->hd);
    vmvar *prefix = alcvar_str(ctx, l->prefix->hd);
    int startch = l->start;

    if (l->token != XPATH_TK_NAME) {
        xpath_set_syntax_error(__LINE__, ctx, l);
        return NULL;
    }
    next_token(ctx, l);
    if (l->token != XPATH_TK_LP) {
        xpath_set_syntax_error(__LINE__, ctx, l);
        return NULL;
    }
    next_token(ctx, l);

    if (l->token != XPATH_TK_RP) {
        for ( ; ; ) {
            array_push(ctx, args, xpath_expr(ctx, l));
            if (l->token != XPATH_TK_COMMA) {
                if (l->token != XPATH_TK_RP) {
                    xpath_set_syntax_error(__LINE__, ctx, l);
                    return NULL;
                }
                break;
            }
            next_token(ctx, l);
        }
    }

    next_token(ctx, l);
    push_posinfo(l, startch, l->prevend);
    vmvar *n = xpath_make_function(ctx, prefix, name, args);
    pop_posinfo(l);
    return n;
}

static vmvar *xpath_primary_expr(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = NULL;
    switch (l->token) {
    case XPATH_TK_STR:
        n = alcvar_str(ctx, l->str->hd);
        next_token(ctx, l);
        break;
    case XPATH_TK_INT: {
        int64_t i = strtoll(l->num->hd, NULL, 10);
        n = alcvar_int64(ctx, i, 0);
        next_token(ctx, l);
        break;
    }
    case XPATH_TK_NUM: {
        double d = strtod(l->num->hd, NULL);
        n = alcvar_double(ctx, &d);
        next_token(ctx, l);
        break;
    }
    case XPATH_TK_LP:
        next_token(ctx, l);
        n = xpath_expr(ctx, l);
        if (l->token != XPATH_TK_RP) {
            xpath_set_syntax_error(__LINE__, ctx, l);
            return NULL;
        }
        next_token(ctx, l);
        break;
    default:
        n = xpath_function_call(ctx, l);
        break;
    }
    return n;
}

static vmvar *xpath_filter_expr(vmctx *ctx, xpath_lexer *l)
{
    int startch = l->start;
    vmvar *n = xpath_primary_expr(ctx, l);
    int endch = l->prevend;

    while (n && l->token == XPATH_TK_LB) {
        push_posinfo(l, startch, endch);
        vmvar *n2 = xpath_predicate(ctx, l);
        if (!n2) return NULL;
        n = xpath_make_predicate(ctx, n, n2, 0);
        pop_posinfo(l);
    }

    return n;
}

static vmvar *xpath_path_expr(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = NULL;
    if (is_primary_expr(l)) {
        int startch = l->start;
        n = xpath_filter_expr(ctx, l);
        if (!n) return NULL;
        int endch = l->prevend;

        if (l->token == XPATH_TK_SLASH) {
            next_token(ctx, l);
            push_posinfo(l, startch, endch);
            vmvar *n2 = xpath_relative_location_path(ctx, l);
            if (!n2) return NULL;
            n = xpath_make_step(ctx, n, n2);
            pop_posinfo(l);
        } else if (l->token == XPATH_TK_SLASHSLASH) {
            next_token(ctx, l);
            push_posinfo(l, startch, endch);
            vmvar *n2 = xpath_relative_location_path(ctx, l);
            if (!n2) return NULL;
            n = xpath_make_step(ctx, n,
                xpath_make_step(ctx,
                    xpath_make_axis(ctx, XPATH_AXIS_DESCENDANT_OR_SELF, XPATH_NODE_ALL, NULL, NULL), n2
                )
            );
            pop_posinfo(l);
        }
    } else {
        n = xpath_location_path(ctx, l);
    }
    return n;
}

static vmvar *xpath_union_expr(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = xpath_path_expr(ctx, l);
    while (n && l->token == XPATH_TK_UNION) {
        next_token(ctx, l);
        vmvar *n2 = xpath_path_expr(ctx, l);
        if (!n2) return NULL;
        n = xpath_make_binop(ctx, XPATH_TK_UNION, n, n2);
    }
    return n;
}

static vmvar *xpath_parse(vmctx *ctx, xpath_lexer *l)
{
    return xpath_expr(ctx, l);
}

static int xpath_compile(vmctx *ctx, vmvar *r, const char *expr)
{
    xpath_lexer lexer = {
        .name = alcstr_str(ctx, ""),
        .prefix = alcstr_str(ctx, ""),
        .str = alcstr_str(ctx, ""),
        .num = alcstr_str(ctx, ""),
        .expr = expr,
        .exprlen = strlen(expr),
        .token = XPATH_TK_UNKNOWN,
        .curidx = -1,
    };
    next_char(&lexer);
    next_token(ctx, &lexer);
    vmvar *root = xpath_parse(ctx, &lexer);
    SHCOPY_VAR_TO(ctx, r, root);
    return lexer.e;
}

static int XPath_compile(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    return xpath_compile(ctx, r, a0->s->hd);
}

static int XPath_opName(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_INT64);
    switch (a0->i) {
    case XPATH_TK_LOR:    SET_STR(r, "or");      break;
    case XPATH_TK_LAND:   SET_STR(r, "and");     break;
    case XPATH_TK_EQ:     SET_STR(r, "eq");      break;
    case XPATH_TK_NE:     SET_STR(r, "ne");      break;
    case XPATH_TK_LT:     SET_STR(r, "lt");      break;
    case XPATH_TK_LE:     SET_STR(r, "le");      break;
    case XPATH_TK_GT:     SET_STR(r, "gt");      break;
    case XPATH_TK_GE:     SET_STR(r, "ge");      break;
    case XPATH_TK_ADD:    SET_STR(r, "add");     break;
    case XPATH_TK_SUB:    SET_STR(r, "sub");     break;
    case XPATH_TK_MUL:    SET_STR(r, "mul");     break;
    case XPATH_TK_DIV:    SET_STR(r, "div");     break;
    case XPATH_TK_MOD:    SET_STR(r, "mod");     break;
    case XPATH_TK_UMINUS: SET_STR(r, "uminus");  break;
    case XPATH_TK_UNION:  SET_STR(r, "union");   break;
    default:              SET_STR(r, "UNKNOWN"); break;
    }
    return 0;
}

static const char *xpath_axis_name(int axis)
{
    switch (axis) {
    case XPATH_AXIS_ANCESTOR:           return "ancestor";
    case XPATH_AXIS_ANCESTOR_OR_SELF:   return "ancestor-or-self";
    case XPATH_AXIS_ATTRIBUTE:          return "attribute";
    case XPATH_AXIS_CHILD:              return "child";
    case XPATH_AXIS_DESCENDANT:         return "descendant";
    case XPATH_AXIS_DESCENDANT_OR_SELF: return "descendant-or-self";
    case XPATH_AXIS_FOLLOWING:          return "following";
    case XPATH_AXIS_FOLLOWING_SIBLING:  return "following-sibling";
    case XPATH_AXIS_NAMESPACE:          return "namespace";
    case XPATH_AXIS_PARENT:             return "parent";
    case XPATH_AXIS_PRECEDING:          return "preceding";
    case XPATH_AXIS_PRECEDING_SIBLING:  return "preceding-sibling";
    case XPATH_AXIS_SELF:               return "self";
    case XPATH_AXIS_ROOT:               return "root";
    default:
        break;
    }
    return "UNKNOWN";
}

static int XPath_axisName(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_INT64);
    SET_STR(r, xpath_axis_name(a0->i));
    return 0;
}

static int XPath_nodeTypeName(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_INT64);
    switch (a0->i) {
    case XPATH_NODE_ROOT:                   SET_STR(r, "root");                   break;
    case XPATH_NODE_ELEMENT:                SET_STR(r, "element");                break;
    case XPATH_NODE_ATTRIBUTE:              SET_STR(r, "attribute");              break;
    case XPATH_NODE_NAMESPACE:              SET_STR(r, "namespace");              break;
    case XPATH_NODE_TEXT:                   SET_STR(r, "text");                   break;
    case XPATH_NODE_PROCESSIGN_INSTRUCTION: SET_STR(r, "processing-instruction"); break;
    case XPATH_NODE_COMMENT:                SET_STR(r, "comment");                break;
    case XPATH_NODE_ALL:                    SET_STR(r, "all");                    break;
    default:                                SET_STR(r, "UNKNOWN");                break;
    }
    return 0;
}

/* evaluate */

static int xpath_evaluate_step(vmctx *ctx, vmvar *r, vmvar *xpath, vmvar *root, vmobj *info, vmobj *base);

static void xpath_print_current_nodeset(vmctx *ctx, vmobj *info, const char *msg)
{
    printf("- %s\n", msg);
    for (int i = 0, len = info->idxsz; i < len; ++i) {
        vmvar *node = info->ary[i];
        printf("  * [%d]: ", i);
        if (node->t == VAR_OBJ) {
            const char *value = hashmap_getstr(node->o, "tagName");
            if (!value) {
                value = hashmap_getstr(node->o, "nodeValue");
            }
            printf("XML node (%s[%d])", value ? value : "unknown", i + 1);
        } else {
            print_obj(ctx, node);
        }
        printf("\n");
    }
}

static vmobj *xpath_make_initial_info(vmctx *ctx, vmobj *node)
{
    vmobj *info = alcobj(ctx);
    array_push(ctx, info, alcvar_obj(ctx, node));
    return info;
}

static void xpath_reset_collected_nodes(vmctx *ctx, vmobj *nodes)
{
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nodev = nodes->ary[i];
        if (nodev->t == VAR_OBJ) {
            nodev->o->is_checked = 0;
        }
    }
}

static int xpath_is_matched(vmctx *ctx, vmobj *node, vmstr *prefix, vmstr *name)
{
    vmstr *qname = NULL;
    if (prefix && prefix->len > 0) {
        qname = alcstr_str(ctx, prefix->hd);
        str_append_cp(ctx, qname, name->hd);
    } else {
        qname = alcstr_str(ctx, name->hd);
    }
    vmvar *tagname = hashmap_search(node, "tagName");
    if (!tagname || tagname->t != VAR_STR) {
        return 0;
    }
    return strcmp(qname->hd, tagname->s->hd) == 0;
}

static int xpath_is_matched_node_type(vmctx *ctx, vmobj *node, int node_type)
{
    vmvar *nodet = hashmap_search(node, "nodeType");
    if (nodet->t != VAR_INT64) {
        return 0;
    }
    switch (node_type) {
    case XPATH_NODE_ROOT:                   return 0;
    case XPATH_NODE_ELEMENT:                return nodet->i == XMLDOM_ELEMENT_NODE;
    case XPATH_NODE_ATTRIBUTE:              return nodet->i == XMLDOM_ATTRIBUTE_NODE;
    case XPATH_NODE_NAMESPACE:              return nodet->i == 0;
    case XPATH_NODE_TEXT:                   return nodet->i == XMLDOM_TEXT_NODE;
    case XPATH_NODE_PROCESSIGN_INSTRUCTION: return nodet->i == XMLDOM_PROCESSING_INSTRUCTION_NODE;
    case XPATH_NODE_COMMENT:                return nodet->i == XMLDOM_COMMENT_NODE;
    case XPATH_NODE_ALL:                    return 1;
    }
    return 0;
}

static void xpath_add_item_if_matched(vmctx *ctx, vmobj *r, vmvar *nodev, vmobj *xpath)
{
    if (nodev->t != VAR_OBJ) return;
    vmobj *node = nodev->o;
    vmstr *prefix = (xpath->ary[2] && xpath->ary[2]->t == VAR_STR) ? xpath->ary[2]->s : NULL;
    int has_prefix = prefix && prefix->hd && *prefix->hd != 0;
    vmstr *name = (xpath->ary[3] && xpath->ary[3]->t == VAR_STR) ? xpath->ary[3]->s : NULL;
    int has_name = name && name->hd && *name->hd != 0;
    int node_type = ((has_prefix || has_name) && xpath->ary[1] && xpath->ary[1]->t == VAR_INT64) ? xpath->ary[1]->i : XPATH_NODE_ALL;
    int is_collected = node->is_checked;
    if (!is_collected &&
            (node_type == XPATH_NODE_ALL ||
            (xpath_is_matched_node_type(ctx, node, node_type) && xpath_is_matched(ctx, node, prefix, name)))) {
        array_push(ctx, r, nodev);
        nodev->o->is_checked = 1;
    }
}

static int xpath_collect_attribute(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    vmobj *xo = xpath->o;
    int node_type = (xo->ary[1] && xo->ary[1]->t == VAR_INT64) ? xo->ary[1]->i : XPATH_NODE_ALL;
    vmstr *prefix = (xo->ary[2] && xo->ary[2]->t == VAR_STR) ? xo->ary[2]->s : NULL;
    vmstr *name = (xo->ary[3] && xo->ary[3]->t == VAR_STR) ? xo->ary[3]->s : NULL;

    vmstr *qname = NULL;
    if (prefix && prefix->len > 0) {
        qname = alcstr_str(ctx, prefix->hd);
        str_append_cp(ctx, qname, ":");
        str_append_cp(ctx, qname, name->hd);
    } else {
        qname = alcstr_str(ctx, name->hd);
    }

    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (!nv || nv->t != VAR_OBJ) continue;
        vmobj *node = nv->o;
        vmvar *av = hashmap_search(node, "attributes");
        if (!av || av->t != VAR_OBJ) continue;
        vmobj *attr = av->o;
        vmobj *keys = object_get_keys(ctx, attr);
        int klen = keys->idxsz;
        for (int j = 0; j < klen; ++j) {
            vmvar *key = keys->ary[j];
            if (key->t != VAR_STR) continue;
            vmvar *val = hashmap_search(attr, key->s->hd);
            if (node_type == XPATH_NODE_ALL) {
                vmobj *kv = alcobj(ctx);
                array_push(ctx, kv, copy_var(ctx, key, 0));
                array_push(ctx, kv, val ? copy_var(ctx, val, 0) : alcvar_initial(ctx));
                array_push(ctx, res, alcvar_obj(ctx, kv));
            } else if (val && strcmp(key->s->hd, qname->hd) == 0) {
                array_push(ctx, res, copy_var(ctx, val, 0));
                break;
            }
        }
    }

    return 0;
}

static int xpath_collect_parent(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv->t != VAR_OBJ) continue;
        vmobj *node = nv->o;
        vmvar *parent = hashmap_search(node, "parentNode");
        if (parent && parent->t == VAR_OBJ) {
            xpath_add_item_if_matched(ctx, res, parent, xo);
        }
    }

    return 0;
}

static int xpath_collect_ancestor_nodes(vmctx *ctx, vmvar *xpath, vmobj *node, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    vmobj *xo = xpath->o;
    vmvar *parent = hashmap_search(node, "parentNode");
    while (parent && parent->t == VAR_OBJ) {
        xpath_add_item_if_matched(ctx, res, parent, xo);
        parent = hashmap_search(parent->o, "parentNode");
    }

    return 0;
}

static int xpath_collect_ancestor(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv->t != VAR_OBJ) continue;
        vmobj *node = nv->o;
        xpath_collect_ancestor_nodes(ctx, xpath, node, res);
    }

    return 0;
}

static int xpath_collect_ancestor_or_self(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv->t != VAR_OBJ) continue;
        vmobj *node = nv->o;
        xpath_add_item_if_matched(ctx, res, nv, xo);
        xpath_collect_ancestor_nodes(ctx, xpath, node, res);
    }

    return 0;
}

static int xpath_collect_child_nodes(vmctx *ctx, vmvar *xpath, vmobj *node, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    vmobj *xo = xpath->o;
    vmvar *child = hashmap_search(node, "firstChild");
    while (child && child->t == VAR_OBJ) {
        xpath_add_item_if_matched(ctx, res, child, xo);
        child = hashmap_search(child->o, "nextSibling");
    }

    return 0;
}

static int xpath_collect_child(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv->t != VAR_OBJ) continue;
        vmobj *node = nv->o;
        int e = xpath_collect_child_nodes(ctx, xpath, node, res);
        XPATH_CHECK_ERR(e);
    }

    return 0;
}

static int xpath_collect_descendant(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv->t != VAR_OBJ) continue;
        vmobj *node = nv->o;
        xpath_collect_child_nodes(ctx, xpath, node, res);
        vmvar *children = hashmap_search(node, "children");
        if (children->t != VAR_OBJ) continue;
        xpath_collect_descendant(ctx, xpath, children->o, res);
    }

    return 0;
}

static int xpath_collect_descendant_or_self(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv->t != VAR_OBJ) continue;
        vmobj *node = nv->o;
        xpath_add_item_if_matched(ctx, res, nv, xo);
        xpath_collect_child_nodes(ctx, xpath, node, res);
        vmvar *children = hashmap_search(node, "children");
        if (children->t != VAR_OBJ) continue;
        xpath_collect_descendant(ctx, xpath, children->o, res);
    }

    return 0;
}

static int xpath_collect_following_sibling(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv->t != VAR_OBJ) continue;
        vmobj *node = nv->o;

        vmvar *next = hashmap_search(node, "nextSibling");
        while (next && next->t == VAR_OBJ) {
            xpath_add_item_if_matched(ctx, res, next, xo);
            next = hashmap_search(next->o, "nextSibling");
        }
    }

    return 0;
}

static int xpath_collect_following(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv->t != VAR_OBJ) continue;
        vmobj *node = nv->o;
        vmvar *parent = nv;
        while (parent && parent->t == VAR_OBJ) {
            vmvar *next = hashmap_search(parent->o, "nextSibling");
            while (next && next->t == VAR_OBJ) {
                xpath_add_item_if_matched(ctx, res, next, xo);
                next = hashmap_search(next->o, "nextSibling");
            }
            parent = hashmap_search(parent->o, "parentNode");
        }
    }

    return 0;
}

static int xpath_collect_preceding_sibling(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv->t != VAR_OBJ) continue;
        vmobj *node = nv->o;

        vmvar *prev = hashmap_search(node, "previousSibling");
        while (prev && prev->t == VAR_OBJ) {
            xpath_add_item_if_matched(ctx, res, prev, xo);
            prev = hashmap_search(prev->o, "previousSibling");
        }
    }

    return 0;
}

static int xpath_collect_preceding(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv->t != VAR_OBJ) continue;
        vmobj *node = nv->o;

        vmvar *parent = nv;
        while (parent && parent->t == VAR_OBJ) {
            vmvar *prev = hashmap_search(parent->o, "previousSibling");
            while (prev && prev->t == VAR_OBJ) {
                xpath_add_item_if_matched(ctx, res, prev, xo);
                prev = hashmap_search(prev->o, "previousSibling");
            }
            parent = hashmap_search(parent->o, "parentNode");
        }
    }

    return 0;
}

static int xpath_collect_self(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv->t != VAR_OBJ) continue;
        xpath_add_item_if_matched(ctx, res, nv, xo);
    }

    return 0;
}

static int xpath_proc_axis(vmctx *ctx, vmvar *r, vmvar *axisv, vmvar *xpath, vmvar *root, vmobj *info)
{
    int e = 0;
    int axis = axisv->t == VAR_INT64 ? axisv->i : XPATH_AXIS_UNKNOWN;
    // xpath_print_current_nodeset(ctx, info, xpath_axis_name(axis));
    vmobj *res = alcobj(ctx);
    switch (axis) {
    case XPATH_AXIS_ANCESTOR:
        e = xpath_collect_ancestor(ctx, xpath, info, res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_ANCESTOR_OR_SELF:
        e = xpath_collect_ancestor_or_self(ctx, xpath, info, res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_ATTRIBUTE:
        e = xpath_collect_attribute(ctx, xpath, info, res);
        SET_OBJ(r, res);
        break;
    case XPATH_AXIS_CHILD:
        e = xpath_collect_child(ctx, xpath, info, res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_DESCENDANT:
        e = xpath_collect_descendant(ctx, xpath, info, res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_DESCENDANT_OR_SELF:
        e = xpath_collect_descendant_or_self(ctx, xpath, info, res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_FOLLOWING:
        e = xpath_collect_following(ctx, xpath, info, res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_FOLLOWING_SIBLING:
        e = xpath_collect_following_sibling(ctx, xpath, info, res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_NAMESPACE:
        /* TODO */
        break;
    case XPATH_AXIS_PARENT:
        e = xpath_collect_parent(ctx, xpath, info, res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_PRECEDING:
        e = xpath_collect_preceding(ctx, xpath, info, res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_PRECEDING_SIBLING:
        e = xpath_collect_preceding_sibling(ctx, xpath, info, res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_SELF:
        e = xpath_collect_self(ctx, xpath, info, res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_ROOT: {
        array_push(ctx, res, root);
        SET_OBJ(r, res);
        break;
    }
    default:
        break;
    }
    // xpath_print_current_nodeset(ctx, res, xpath_axis_name(axis)); printf("\n");
    return e;
}

static int xpath_to_boolean(vmvar *v)
{
    XPATH_EXPAND_V(v);
    switch (v->t) {
    case VAR_BOOL:
        return v->i != 0;
    case VAR_INT64:
        v->t = VAR_BOOL;
        return v->i != 0;
    case VAR_DBL:
        v->t = VAR_BOOL;
        v->i = v->d >= DBL_EPSILON;
        return v->i != 0;
    case VAR_BIG:
        v->t = VAR_BOOL;
        v->i = BzToDouble(v->bi->b) >= DBL_EPSILON;
        return v->i != 0;
    case VAR_STR:
        v->t = VAR_BOOL;
        v->i = v->s->len > 0;
        return v->i != 0;
    case VAR_OBJ:
        v->t = VAR_BOOL;
        v->i = v->o->idxsz > 0;
        return v->i != 0;
    default:
        break;
    }
    return 0;
}

static void xpath_to_number(vmvar *v)
{
    XPATH_EXPAND_V(v);
    switch (v->t) {
    case VAR_BOOL:
        v->t = VAR_INT64;
        break;
    case VAR_INT64:
    case VAR_DBL:
        break;
    case VAR_BIG:
        v->t = VAR_DBL;
        v->d = BzToDouble(v->bi->b);
        break;
    case VAR_STR:
        v->t = VAR_INT64;
        v->i = strtoll(v->s->hd, NULL, 10);
        break;
    default:
        v->t = VAR_INT64;
        v->i = INT64_MIN;
        break;
    }
}

static void xpath_to_string(vmctx *ctx, vmvar *v)
{
    XPATH_EXPAND_V(v);
    switch (v->t) {
    case VAR_BOOL:
    case VAR_INT64: {
        int64_t i = v->i;
        v->t = VAR_STR;
        v->s = alcstr_str(ctx, "");
        str_append_i64(ctx, v->s, i);
        break;
    }
    case VAR_BIG: {
        char *bs = BzToString(v->bi->b, 10, 0);
        v->t = VAR_STR;
        v->s = alcstr_str(ctx, bs);
        BzFreeString(bs);
        break;
    }
    case VAR_DBL: {
        double d = v->d;
        v->t = VAR_STR;
        v->s = alcstr_str(ctx, "");
        str_append_dbl(ctx, v->s, &d);
        break;
    }
    default:
        v->t = VAR_STR;
        v->s = alcstr_str(ctx, "");
        break;
    }
}

static void xpath_to_comp_eq(vmctx *ctx, vmvar *lhs, vmvar *rhs)
{
    XPATH_EXPAND_V(lhs);
    XPATH_EXPAND_V(rhs);
    if (lhs->t == VAR_OBJ) {
        vmstr *text = alcstr_str(ctx, "");
        XmlDom_getTextContent(ctx, text, lhs);
        SET_SV(lhs, text);
    }
    if (rhs->t == VAR_OBJ) {
        vmstr *text = alcstr_str(ctx, "");
        XmlDom_getTextContent(ctx, text, rhs);
        SET_SV(rhs, text);
    }
    int l_is_num = xpath_is_number(lhs);
    int r_is_num = xpath_is_number(rhs);
    if (l_is_num || r_is_num) {
        if (!l_is_num) {
            xpath_to_number(lhs);
        }
        if (!r_is_num) {
            xpath_to_number(rhs);
        }
    } else {
        if (lhs->t != VAR_STR) {
            xpath_to_string(ctx, lhs);
        }
        if (rhs->t != VAR_STR) {
            xpath_to_string(ctx, rhs);
        }
    }
}

static int xpath_evaluate_expr(vmctx *ctx, vmvar *r, int op, vmvar *lhs, vmvar *rhs)
{
    int e = 0;
    switch (op) {
    case XPATH_OP_EQ:
        xpath_to_comp_eq(ctx, lhs, rhs);
        OP_EQEQ(ctx, r, lhs, rhs, L0, __func__, __FILE__, 0);
        return e;
    case XPATH_OP_NE:
        xpath_to_comp_eq(ctx, lhs, rhs);
        OP_NEQ(ctx, r, lhs, rhs, L0, __func__, __FILE__, 0);
        return e;
    case XPATH_OP_LT:
        xpath_to_number(lhs);
        xpath_to_number(rhs);
        OP_LT(ctx, r, lhs, rhs, L0, __func__, __FILE__, 0);
        return e;
    case XPATH_OP_LE:
        xpath_to_number(lhs);
        xpath_to_number(rhs);
        OP_LE(ctx, r, lhs, rhs, L0, __func__, __FILE__, 0);
        return e;
    case XPATH_OP_GT:
        xpath_to_number(lhs);
        xpath_to_number(rhs);
        OP_GT(ctx, r, lhs, rhs, L0, __func__, __FILE__, 0);
        return e;
    case XPATH_OP_GE:
        xpath_to_number(lhs);
        xpath_to_number(rhs);
        OP_GE(ctx, r, lhs, rhs, L0, __func__, __FILE__, 0);
        return e;
    case XPATH_OP_ADD:
        xpath_to_number(lhs);
        xpath_to_number(rhs);
        OP_ADD(ctx, r, lhs, rhs, L0, __func__, __FILE__, 0);
        return e;
    case XPATH_OP_SUB:
        xpath_to_number(lhs);
        xpath_to_number(rhs);
        OP_SUB(ctx, r, lhs, rhs, L0, __func__, __FILE__, 0);
        return e;
    case XPATH_OP_MUL:
        xpath_to_number(lhs);
        xpath_to_number(rhs);
        OP_MUL(ctx, r, lhs, rhs, L0, __func__, __FILE__, 0);
        return e;
    case XPATH_OP_DIV:
        xpath_to_number(lhs);
        xpath_to_number(rhs);
        OP_DIV(ctx, r, lhs, rhs, L0, __func__, __FILE__, 0);
        return e;
    case XPATH_OP_MOD:
        xpath_to_number(lhs);
        xpath_to_number(rhs);
        OP_MOD(ctx, r, lhs, rhs, L0, __func__, __FILE__, 0);
        return e;
    case XPATH_OP_UMINUS:
        xpath_to_number(lhs);
        OP_UMINUS(ctx, r, lhs, L0, __func__, __FILE__, 0);
        return e;
    case XPATH_OP_UNION:
        break;
    }
L0:;
    return e;
}

static int xpath_proc_expr(vmctx *ctx, int op, vmvar *r, vmvar *lhs, vmvar *rhs, vmobj *node, vmvar *root, vmobj *base)
{
    int e = 0;
    vmvar *linfo = alcvar_initial(ctx);
    vmobj *info = xpath_make_initial_info(ctx, node);
    e = xpath_evaluate_step(ctx, linfo, lhs, root, info, base);
    XPATH_CHECK_ERR(e);
    XPATH_EXPAND_V(linfo);
    switch (op) {
    case XPATH_OP_LOR: {
        int b = xpath_to_boolean(linfo);
        if (b) {
            SET_BOOL(r, 1);
        } else {
            vmvar *rinfo = alcvar_initial(ctx);
            info = xpath_make_initial_info(ctx, node);
            e = xpath_evaluate_step(ctx, rinfo, rhs, root, info, base);
            XPATH_CHECK_ERR(e);
            XPATH_EXPAND_V(rinfo);
            b = xpath_to_boolean(rinfo);
            SET_BOOL(r, (b ? 1 : 0));
        }
        break;
    }
    case XPATH_OP_LAND: {
        int b = xpath_to_boolean(linfo);
        if (b) {
            vmvar *rinfo = alcvar_initial(ctx);
            info = xpath_make_initial_info(ctx, node);
            e = xpath_evaluate_step(ctx, rinfo, rhs, root, info, base);
            XPATH_CHECK_ERR(e);
            XPATH_EXPAND_V(rinfo);
            b = xpath_to_boolean(rinfo);
            SET_BOOL(r, (b ? 1 : 0));
        } else {
            SET_BOOL(r, 0);
        }
        break;
    }
    default: {
        vmvar *rinfo = alcvar_initial(ctx);
        info = xpath_make_initial_info(ctx, node);
        e = xpath_evaluate_step(ctx, rinfo, rhs, root, info, base);
        XPATH_CHECK_ERR(e);
        e = xpath_evaluate_expr(ctx, r, op, linfo, rinfo);
        break;
    }}

    return e;
}

static int xpath_function_position(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    int position = -1;
    for (int i = 0, len = base->idxsz; i < len; ++i) {
        if (base->ary[i] && base->ary[i]->t == VAR_OBJ) {
            if (base->ary[i]->o == node) {
                position = i + 1;
                break;
            }
        }
    }
    SET_I64(r, position);
    return 0;
}

static int xpath_function_last(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    SET_I64(r, base ? base->idxsz : -1);
    return 0;
}

static int xpath_function_count(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz == 0) {
        SET_I64(r, 0);
        return 0;
    }
    vmvar *arg0 = args->ary[0];
    if (arg0->t != VAR_OBJ) {
        SET_I64(r, 0);
        return 0;
    }
    vmobj *nodes = alcobj(ctx);
    array_push(ctx, nodes, alcvar_obj(ctx, node));
    int e = xpath_evaluate_step(ctx, r, arg0, root, nodes, base);
    if (e == 0) {
        if (r->t == VAR_OBJ) {
            SET_I64(r, r->o->idxsz);
        } else {
            SET_I64(r, 0);
        }
    }
    return e;
}

static int xpath_proc_function_call(vmctx *ctx, vmvar *r, vmstr *func, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    int e = 0;
    const char *funcname = func->hd;
    if (strcmp(funcname, "position") == 0) {
        e = xpath_function_position(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "last") == 0) {
        e = xpath_function_last(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "count") == 0) {
        e = xpath_function_count(ctx, r, args, root, node, base);
    } else {
        return throw_system_exception(__LINE__, ctx, EXCEPT_XML_ERROR, "Unsupported function name");
    }
    return e;
}

static int xpath_evaluate_predicate(vmctx *ctx, vmobj *resobj, vmvar *xpath, vmvar *root, vmobj *info)
{
    int e = 0;
    vmobj *nodes = alcobj(ctx);
    for (int i = 0, len = info->idxsz; i < len; ++i) {
        vmvar *node = info->ary[i];
        if (!node || node->t != VAR_OBJ) continue;
        vmvar r = {0};
        array_set(ctx, nodes, 0, node);
        e = xpath_evaluate_step(ctx, &r, xpath, root, nodes, info);
        if (e != 0) break;
        if (r.t == VAR_BOOL) {
            if (r.i != 0) {
                array_push(ctx, resobj, node);
            }
        } else if (r.t == VAR_INT64) {
            int position = i + 1;
            if (0 < position && r.i == position) {
                array_push(ctx, resobj, node);
            }
        }
    }
    return e;
}


static int xpath_evaluate_step(vmctx *ctx, vmvar *r, vmvar *xpath, vmvar *root, vmobj *info, vmobj *base)
{
    int e = 0;
    switch (xpath->t) {
    case VAR_INT64:
        SET_I64(r, xpath->i);
        break;
    case VAR_DBL:
        SET_DBL(r, xpath->d);
        break;
    case VAR_STR:
        SET_SV(r, xpath->s);
        break;
    case VAR_OBJ: {
        vmobj *xobj = xpath->o;
        int optype = hashmap_getint(xobj, "op", XPATH_OP_UNKNOWN);
        switch (optype) {
        case XPATH_OP_STEP: {
            e = xpath_evaluate_step(ctx, r, xobj->ary[0], root, info, base);
            XPATH_CHECK_ERR(e);
            XPATH_CHECK_OBJ(r);
            vmobj *nodes = r->o;
            e = xpath_evaluate_step(ctx, r, xobj->ary[1], root, nodes, base);
            break;
        }
        case XPATH_OP_AXIS:
            e = xpath_proc_axis(ctx, r, xobj->ary[0], xpath, root, info);
            break;
        case XPATH_OP_PREDICATE: {
            e = xpath_evaluate_step(ctx, r, xobj->ary[0], root, info, base);
            XPATH_CHECK_ERR(e);
            XPATH_CHECK_OBJ(r);
            vmobj *nodes = r->o;
            vmvar *predvalue = xobj->ary[1];
            vmobj *resobj = alcobj(ctx);
            if (predvalue->t == VAR_INT64) {
                int index = predvalue->i;
                if (0 < index && index <= nodes->idxsz) {
                    array_push(ctx, resobj, nodes->ary[index - 1]);
                }
            } else {
                e = xpath_evaluate_predicate(ctx, resobj, predvalue, root, nodes);
                XPATH_CHECK_ERR(e);
            }
            SET_OBJ(r, resobj);
            break;
        }
        case XPATH_OP_EXPR: {
            XPATH_EXPAND_O(info);
            if (!info) {
                return xpath_set_invalid_state_exception(__LINE__, ctx);
            }
            vmvar *xobjv0 = xobj->ary[0];
            int op = xobjv0->t == VAR_INT64 ? xobjv0->i : XPATH_OP_UNKNOWN;
            e = xpath_proc_expr(ctx, op, r, xobj->ary[1], xobj->ary[2], info, root, base);
            break;
        }
        case XPATH_OP_FUNCCALL: {
            XPATH_EXPAND_O(info);
            if (!info) {
                return xpath_set_invalid_state_exception(__LINE__, ctx);
            }
            vmvar *func = xobj->ary[1];
            if (func->t != VAR_STR) {
                return throw_system_exception(__LINE__, ctx, EXCEPT_XML_ERROR, "Invalid function name");
            }
            vmvar *args = xobj->ary[2];
            e = xpath_proc_function_call(ctx, r, func->s, (args && args->t == VAR_OBJ) ? args->o : NULL, root, info, base);
            break;
        }
        default:
            return xpath_set_invalid_state_exception(__LINE__, ctx);
        }
        break;
    }
    default:
        return xpath_set_invalid_state_exception(__LINE__, ctx);
    }

    return e;
}

static int XPath_xpath_hook(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(nodeset, 0, VAR_OBJ);
    DEF_ARG(str, 1, VAR_STR);

    vmobj *nodes = nodeset->o;
    if (nodes->idxsz == 0 || !(nodes->ary[0] && nodes->ary[0]->t == VAR_OBJ)) {
        SET_OBJ(r, alcobj(ctx));
        return 0;
    }

    vmvar *root = NULL;
    vmvar *n1 = nodes->ary[0];
    if (n1->t == VAR_OBJ) {
        vmvar *doc = hashmap_search(n1->o, "ownerDocument");
        if (doc->t == VAR_OBJ) {
            root = hashmap_search(doc->o, "documentElement");
        }
    }
    if (!root) {
        SET_OBJ(r, alcobj(ctx));
        return 0;
    }

    vmvar xpath = {0};
    xpath_compile(ctx, &xpath, str->s->hd);
    int e = xpath_evaluate_step(ctx, r, &xpath, root, nodes, NULL);
    mark_and_sweep(ctx);
    return e;
}

static int XPath_evaluate(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(node, 0, VAR_OBJ);
    DEF_ARG(str, 1, VAR_STR);

    vmvar *docelem = hashmap_search(node->o, "documentElement");
    if (docelem) {
        node = docelem;
    }

    vmvar xpath = {0};
    xpath_compile(ctx, &xpath, str->s->hd);
    vmobj *info = xpath_make_initial_info(ctx, node->o);
    int e = xpath_evaluate_step(ctx, r, &xpath, node, info, NULL);
    mark_and_sweep(ctx);
    return e;
}

int XPath(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    /* Extending the Array method for XPath. */
    KL_SET_METHOD(ctx->o, xpath, XPath_xpath_hook, lex, 2);

    vmobj *o = alcobj(ctx);
    KL_SET_METHOD(o, create, XPath_compile, lex, 1);
    KL_SET_METHOD(o, compile, XPath_compile, lex, 1);
    KL_SET_METHOD(o, opName, XPath_opName, lex, 1);
    KL_SET_METHOD(o, axisName, XPath_axisName, lex, 1);
    KL_SET_METHOD(o, nodeTypeName, XPath_nodeTypeName, lex, 1);
    KL_SET_METHOD(o, evaluate, XPath_evaluate, lex, 1);

    vmobj *axis = alcobj(ctx);
    KL_SET_PROPERTY_I(axis, ANCESTOR,             XPATH_AXIS_ANCESTOR);
    KL_SET_PROPERTY_I(axis, ANCESTOR_OR_SELF,     XPATH_AXIS_ANCESTOR_OR_SELF);
    KL_SET_PROPERTY_I(axis, ATTRIBUTE,            XPATH_AXIS_ATTRIBUTE);
    KL_SET_PROPERTY_I(axis, CHILD,                XPATH_AXIS_CHILD);
    KL_SET_PROPERTY_I(axis, DESCENDANT,           XPATH_AXIS_DESCENDANT);
    KL_SET_PROPERTY_I(axis, DESCENDANT_OR_SELF,   XPATH_AXIS_DESCENDANT_OR_SELF);
    KL_SET_PROPERTY_I(axis, FOLLOWING,            XPATH_AXIS_FOLLOWING);
    KL_SET_PROPERTY_I(axis, FOLLOWING_SIBLING,    XPATH_AXIS_FOLLOWING_SIBLING);
    KL_SET_PROPERTY_I(axis, NAMESPACE,            XPATH_AXIS_NAMESPACE);
    KL_SET_PROPERTY_I(axis, PARENT,               XPATH_AXIS_PARENT);
    KL_SET_PROPERTY_I(axis, PRECEDING,            XPATH_AXIS_PRECEDING);
    KL_SET_PROPERTY_I(axis, PRECEDING_SIBLING,    XPATH_AXIS_PRECEDING_SIBLING);
    KL_SET_PROPERTY_I(axis, SELF,                 XPATH_AXIS_SELF);
    KL_SET_PROPERTY_I(axis, ROOT,                 XPATH_AXIS_ROOT);
    KL_SET_PROPERTY_O(o, axis, axis);

    vmobj *nt = alcobj(ctx);
    KL_SET_PROPERTY_I(nt, ROOT,                   XPATH_NODE_ROOT);
    KL_SET_PROPERTY_I(nt, ELEMENT,                XPATH_NODE_ELEMENT);
    KL_SET_PROPERTY_I(nt, ATTRIBUTE,              XPATH_NODE_ATTRIBUTE);
    KL_SET_PROPERTY_I(nt, NAMESPACE,              XPATH_NODE_NAMESPACE);
    KL_SET_PROPERTY_I(nt, TEXT,                   XPATH_NODE_TEXT);
    KL_SET_PROPERTY_I(nt, PROCESSIGN_INSTRUCTION, XPATH_NODE_PROCESSIGN_INSTRUCTION);
    KL_SET_PROPERTY_I(nt, COMMENT,                XPATH_NODE_COMMENT);
    KL_SET_PROPERTY_I(nt, ALL,                    XPATH_NODE_ALL);
    KL_SET_PROPERTY_O(o, nodeType, nt);

    SET_OBJ(r, o);
    return 0;
}
