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
static const char *xmldom_parse_doc(vmctx *ctx, vmfrm *lex, vmvar *doc, vmobj *nsmap, vmvar *r, const char *p, xmlctx *xctx);
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

static int xmldom_is_same_tagname(const char *t, int tlen, const char *c, int clen)
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

static const char *xmldom_gen_intval(int *v, int *err, const char *s, int radix)
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

static vmvar *xmldom_make_pure_str_obj(vmctx *ctx, int *err, const char *s, int len)
{
    char *buf = alloca(len + 2);
    strncpy(buf, s, len);
    buf[len] = 0;
    return alcvar_str(ctx, buf);
}

static vmvar *xmldom_make_str_obj(vmctx *ctx, int *err, const char *s, int len)
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
            s = xmldom_gen_intval(&v, err, x ? s+1 : s, x ? 16 : 10);
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

static void xmldom_set_attrs(vmctx *ctx, int *err, vmobj *attrs, const char *n, int nlen, const char *v, int vlen)
{
    vmvar *value = xmldom_make_str_obj(ctx, err, v, vlen);
    if (*err < 0) return;
    char *key = alloca(nlen + 2);
    strncpy(key, n, nlen);
    key[nlen] = 0;
    hashmap_set(ctx, attrs, key, value);
}

static const char *xmldom_parse_attrs(vmctx *ctx, vmobj *nsmap, vmobj *attrs, const char *p, xmlctx *xctx)
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
        vmvar *value = xmldom_make_str_obj(ctx, &(xctx->err), attrv, p - attrv);
        array_set(ctx, nsmap, 0, value);
    } else if (strncmp(attrn, "xmlns:", 6) == 0) {
        xmldom_set_attrs(ctx, &(xctx->err), nsmap, attrn + 6, attrnlen - 6, attrv, p - attrv);
    } else {
        xmldom_set_attrs(ctx, &(xctx->err), attrs, attrn, attrnlen, attrv, p - attrv);
    }
    if (xctx->err < 0) return p;
    if (*p == br) { move_next(p); }
    return p;
}

static const char *xmldom_parse_comment(vmctx *ctx, vmvar *r, const char *p, xmlctx *xctx)
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
    vmvar *comment = xmldom_make_pure_str_obj(ctx, &(xctx->err), s, p - s - 2);
    hashmap_set(ctx, r->o, "nodeName", alcvar_str(ctx, "#comment"));
    hashmap_set(ctx, r->o, "nodeValue", comment);
    hashmap_set(ctx, r->o, "comment", comment);
    return p;
}

static const char *xmldom_parse_pi(vmctx *ctx, vmobj *nsmap, vmvar *doc, vmvar *r, const char *p, xmlctx *xctx, int *xmldecl)
{
    const char *s = p;
    while (!is_whitespace(*p)) {
        end_of_text_error((xctx->err), p);
        move_next(p);
    }
    if (s != p) {
        vmvar *target = xmldom_make_str_obj(ctx, &(xctx->err), s, p - s);
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
        p = xmldom_parse_attrs(ctx, nsmap, attrs, p, xctx);
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

static const char *xmldom_get_namespace_uri(vmctx *ctx, vmobj *nsmap, const char *prefix)
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

static const char *xmldom_parse_node(vmctx *ctx, vmfrm *lex, vmvar *doc, vmobj *nsmap, vmvar *r, const char *p, xmlctx *xctx)
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
    vmvar *lname = xmldom_make_str_obj(ctx, &(xctx->err), tag, taglen);
    if (xctx->err < 0) return p;
    hashmap_set(ctx, r->o, "localName", lname);
    vmvar *nsname = NULL;
    if (nslen > 0) {
        nsname = xmldom_make_str_obj(ctx, &(xctx->err), ns, nslen);
        if (xctx->err < 0) return p;
        hashmap_set(ctx, r->o, "prefix", nsname);
        vmvar *tname = xmldom_make_str_obj(ctx, &(xctx->err), ns, nslen + taglen + 1);
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
            p = xmldom_parse_attrs(ctx, nsmap, attrs, p, xctx);
            if (xctx->err < 0) return p;
        }
    }
    hashmap_set(ctx, r->o, "attributes", alcvar_obj(ctx, attrs));

    if (nsname) {
        const char *uri = xmldom_get_namespace_uri(ctx, nsmap, nsname->s->hd);
        hashmap_set(ctx, r->o, "namespaceURI", alcvar_str(ctx, uri));
    } else {
        const char *uri = xmldom_get_namespace_uri(ctx, nsmap, NULL);
        hashmap_set(ctx, r->o, "namespaceURI", alcvar_str(ctx, uri));
    }

    if (*p == '/') { move_next(p); return p; }
    if (*p == '>') { move_next(p); }
    xctx->depth++;
    p = xmldom_parse_doc(ctx, lex, doc, nsmap, r, p, xctx);
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

    if (!xmldom_is_same_tagname(tag, taglen, ctag, ctaglen)) {
        errset_wuth_ret((xctx->err), XMLDOM_ECLOSE);
    }
    if (nslen > 0) {
        if (!xmldom_is_same_tagname(ns, nslen, cns, cnslen)) {
            errset_wuth_ret((xctx->err), XMLDOM_ECLOSE);
        }
    }
    return p;
}

static vmvar *xmldom_checkid(vmvar *node)
{
    vmvar *attr = hashmap_search(node->o, "attributes");
    if (attr && attr->t == VAR_OBJ) {
        return hashmap_search(attr->o, "id");
    }
    return NULL;
}

static vmvar *xmldom_get_element_by_id_node(vmvar *node, const char *idvalue)
{
    vmvar *id = xmldom_checkid(node);
    if (id && id->t == VAR_STR && strcmp(id->s->hd, idvalue) == 0) {
        return node;
    }
    vmobj *c = node->o;
    for (int i = 0, n = c->idxsz; i < n; i++) {
        vmvar *v = xmldom_get_element_by_id_node(c->ary[i], idvalue);
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
    vmvar *found = xmldom_get_element_by_id_node(a0, a1->s->hd);
    if (found && found->t == VAR_OBJ) {
        SET_OBJ(r, found->o);
    } else {
        SET_I64(r, 0);
    }
    return 0;
}

static void xmldom_get_elements_by_tagname_node(vmctx *ctx, vmobj *nodes, vmvar *node, const char *name)
{
    vmvar *tagName = hashmap_search(node->o, "tagName");
    if (tagName && tagName->t == VAR_STR && strcmp(tagName->s->hd, name) == 0) {
        array_push(ctx, nodes, node);
    }
    vmobj *c = node->o;
    for (int i = 0, n = c->idxsz; i < n; i++) {
        xmldom_get_elements_by_tagname_node(ctx, nodes, c->ary[i], name);
    }
}

static int XmlDom_getElementsByTagName(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    DEF_ARG(a1, 1, VAR_STR);
    vmobj *nodes = alcobj(ctx);
    xmldom_get_elements_by_tagname_node(ctx, nodes, a0, a1->s->hd);
    SET_OBJ(r, nodes);
    return 0;
}

static void xmldom_update_node_number(vmvar *node, int *idx)
{
    if (node->t != VAR_OBJ) return;
    vmobj *n = node->o;
    n->value = ++*idx;
    int len = n->idxsz; // child nodes.
    for (int i = 0; i < len; i++) {
        xmldom_update_node_number(n->ary[i], idx);
    }
}

static void xmldom_update_node_number_all(vmvar *node)
{
    if (node->t != VAR_OBJ) return;
    vmvar *doc = hashmap_search(node->o, "ownerDocument");
    if (doc && doc->t == VAR_OBJ) {
        if (doc->o->value > 0) {
            vmvar *root = hashmap_search(doc->o, "documentElement");
            int i = 0;
            xmldom_update_node_number(root, &i);
            doc->o->value = 0;
        }
    }
}

static void xmldom_update_node_number_set_flag_on(vmobj *node)
{
    vmvar *doc = hashmap_search(node, "ownerDocument");
    if (doc && doc->t == VAR_OBJ) {
        doc->o->value = 1;
    }
}

static int xmldom_remove_node(vmctx *ctx, vmvar *parent, vmobj *node)
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

    xmldom_update_node_number_set_flag_on(node);
    return 0;
}

static int XmlDom_remove(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    vmobj *node = a0->o;
    vmvar *parent = hashmap_search(node, "parentNode");

    xmldom_remove_node(ctx, parent, node);

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

    xmldom_update_node_number_set_flag_on(newnode);
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

    xmldom_update_node_number_set_flag_on(newnode);
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

    xmldom_update_node_number_set_flag_on(newnode);
    return 0;
}

static int XmlDom_removeChild(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(parent, 0, VAR_OBJ);
    DEF_ARG(a1, 1, VAR_OBJ);
    vmobj *node = a1->o;

    xmldom_remove_node(ctx, parent, node);

    SET_I64(r, 0);
    return 0;
}

static int xmldom_replace_node_from_parent(vmctx *ctx, vmobj *po, vmobj *node1, vmobj *node2)
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

    node2->value = node1->value;
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

    int e = xmldom_replace_node_from_parent(ctx, parent->o, node1->o, node2->o);
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

    int e = xmldom_replace_node_from_parent(ctx, parent->o, node1, node2);
    if (e != 0) {
        return e;
    }

    SET_I64(r, 0);
    return 0;
}

static void xmldom_get_text_content(vmctx *ctx, vmstr *text, vmvar *node)
{
    int node_type = hashmap_getint(node->o, "nodeType", -1);
    if (node_type == XMLDOM_TEXT_NODE || node_type == XMLDOM_CDATA_SECTION_NODE ||
            node_type == XMLDOM_COMMENT_NODE || node_type == XMLDOM_PROCESSING_INSTRUCTION_NODE) {
        const char *node_value = hashmap_getstr(node->o, "nodeValue");
        if (node_value) {
            str_append_cp(ctx, text, node_value);
        }
        return;
    }

    vmobj *c = node->o;
    for (int i = 0, n = c->idxsz; i < n; i++) {
        xmldom_get_text_content(ctx, text, c->ary[i]);
    }
}

static int XmlDom_textContent(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(node, 0, VAR_OBJ);
    vmstr *text = alcstr_str(ctx, "");
    xmldom_get_text_content(ctx, text, node);
    SET_SV(r, text);
    return 0;
}

static void xmldom_setup_xmlnode_props(vmctx *ctx, vmfrm *lex, vmvar *doc, vmvar *r, vmvar *parent, int index, int type, xmlctx *xctx)
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

static void xmldom_setup_xmldoc_props(vmctx *ctx, vmfrm *lex, vmvar *r)
{
    vmvar *root = NULL;
    vmobj *o = r->o;
    vmvar **ary = o->ary;
    int n = o->idxsz;
    for (int i = 0; i < n; i++) {
        vmobj *vo = ary[i]->o;
        vmvar *v = hashmap_search(vo, "nodeType");
        if (v && v->t == VAR_INT64) {
            if (v->i == XMLDOM_ELEMENT_NODE) {
                root = ary[i];
                KL_SET_PROPERTY(o, documentElement, root);
            }
        }
    }
    KL_SET_PROPERTY_S(o, nodeName, "#document");
    KL_SET_METHOD(o, xpath, XPath_evaluate, lex, 2);
    if (root) {
        int i = 0;
        xmldom_update_node_number(root, &i);
    }
    o->value = 0;   /* No need to update the node number. */
}

static const char *xmldom_parse_doc(vmctx *ctx, vmfrm *lex, vmvar *doc, vmobj *nsmap, vmvar *r, const char *p, xmlctx *xctx)
{
    while (*p != 0) {
        const char *s = p;
        while (*p != '<') {
            if (*p == 0) break;
            move_next(p);
        }
        if (s != p) {
            vmvar *vs = xmldom_make_str_obj(ctx, &(xctx->err), s, p - s);
            if (xctx->err < 0) return p;
            if (xctx->normalize_space) {
                str_trim(ctx, vs->s, " \t\r\n");
            }
            if (!xctx->normalize_space || vs->s->len > 0) {
                vmvar *vo = alcvar_obj(ctx, alcobj(ctx));
                KL_SET_PROPERTY(vo->o, text, vs);
                KL_SET_PROPERTY(vo->o, nodeValue, vs);
                KL_SET_PROPERTY_S(vo->o, nodeName, "#text");
                xmldom_setup_xmlnode_props(ctx, lex, doc, vo, r, r->o->idxsz, XMLDOM_TEXT_NODE, xctx);
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
                p = xmldom_parse_comment(ctx, n, p+1, xctx);
                xmldom_setup_xmlnode_props(ctx, lex, doc, n, r, r->o->idxsz, XMLDOM_COMMENT_NODE, xctx);
            } else if (*p == '?') {
                move_next(p);
                int xmldecl = 0;
                p = xmldom_parse_pi(ctx, cnsmap, r, n, p, xctx, &xmldecl);
                if (xmldecl && (xctx->depth > 0 || r->o->idxsz > 0)) {
                    errset_wuth_ret((xctx->err), XMLDOM_EDECL);
                }
                xmldom_setup_xmlnode_props(ctx, lex, doc, n, r, r->o->idxsz,
                    xmldecl ? XMLDOM_XMLDECL_NODE : XMLDOM_PROCESSING_INSTRUCTION_NODE, xctx);
            } else {
                p = xmldom_parse_node(ctx, lex, doc, cnsmap, n, p, xctx);
                xmldom_setup_xmlnode_props(ctx, lex, doc, n, r, r->o->idxsz, XMLDOM_ELEMENT_NODE, xctx);
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

static void xmldom_check_options(vmobj *opts, xmlctx *xctx)
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
            .normalize_space = 0,
        };
        if (a1->t == VAR_OBJ) {
            xmldom_check_options(a1->o, &xctx);
        }
        xmldom_parse_doc(ctx, lex, r, nsmap, r, str, &xctx);
        xmldom_setup_xmldoc_props(ctx, lex, r);
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
#define XPATH_CHECK_OBJ(v) do { if ((v)->t != VAR_OBJ) return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state"); } while (0)
#define XPATH_CHECK_ERR(e) do { if (e != 0) return e; } while (0)
#define XPATH_EXPAND_V(v) do { if ((v)->t == VAR_OBJ && (v)->o->idxsz > 0) SHCOPY_VAR_TO(ctx, (v), (v)->o->ary[0]); } while (0)
#define XPATH_EXPAND_O(obj) do { if ((obj) && (obj)->idxsz > 0 && (obj)->ary[0]->t == VAR_OBJ) (obj) = (obj)->ary[0]->o; } while (0)

static void xpath_next_char(xpath_lexer *l)
{
    l->curidx++;
    if (l->curidx < l->exprlen) {
        l->cur = l->expr[l->curidx];
    } else {
        l->cur = 0;
    }
}

static void xpah_skip_whitespace(xpath_lexer *l)
{
    while (is_whitespace(l->cur)) {
        xpath_next_char(l);
    }
}

static void xpath_set_index(xpath_lexer *l, int idx)
{
    l->curidx = idx - 1;
    xpath_next_char(l);
}

static int xpath_check_axis(xpath_lexer *l)
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

static int xpath_check_operator(xpath_lexer *l, int asta)
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

static int xpath_get_string(vmctx *ctx, xpath_lexer *l)
{
    int startidx = l->curidx + 1;
    int endch = l->cur;
    xpath_next_char(l);
    while (l->cur != endch && l->cur != 0) {
        xpath_next_char(l);
    }

    if (l->cur == 0) {
        return 0;
    }

    str_set(ctx, l->str, l->expr + startidx, l->curidx - startidx);
    xpath_next_char(l);
    return 1;
}

static int xpath_get_ncname(vmctx *ctx, xpath_lexer *l, vmstr *name)
{
    int startidx = l->curidx;
    while (is_ncname_char(l->cur)) {
        xpath_next_char(l);
    }

    str_set(ctx, name, l->expr + startidx, l->curidx - startidx);
    return 1;
}

static int xpath_get_number(vmctx *ctx, xpath_lexer *l)
{
    int r = XPATH_TK_INT;
    int startidx = l->curidx;
    while (is_ascii_digit(l->cur)) {
        xpath_next_char(l);
    }
    if (l->cur == '.') {
        r = XPATH_TK_NUM;
        xpath_next_char(l);
        while (is_ascii_digit(l->cur)) {
            xpath_next_char(l);
        }
    }
    if ((l->cur & (~0x20)) == 'E') {
        return 0;
    }

    str_set(ctx, l->num, l->expr + startidx, l->curidx - startidx);
    return r;
}

static void xpath_next_token(vmctx *ctx, xpath_lexer *l)
{
    l->prevend = l->cur;
    l->prevtype = l->token;
    xpah_skip_whitespace(l);
    l->start = l->curidx;

    switch (l->cur) {
    case 0:   l->token = XPATH_TK_EOF; return;
    case '(': l->token = XPATH_TK_LP;     xpath_next_char(l); break;
    case ')': l->token = XPATH_TK_RP;     xpath_next_char(l); break;
    case '[': l->token = XPATH_TK_LB;     xpath_next_char(l); break;
    case ']': l->token = XPATH_TK_RB;     xpath_next_char(l); break;
    case '@': l->token = XPATH_TK_AT;     xpath_next_char(l); break;
    case ',': l->token = XPATH_TK_COMMA;  xpath_next_char(l); break;

    case '.':
        xpath_next_char(l);
        if (l->cur == '.') {
            l->token = XPATH_TK_DOTDOT;
            xpath_next_char(l);
        } else if (is_ascii_digit(l->cur)) {
            l->curidx = l->start - 1;
            xpath_next_char(l);
            goto CASE_0;
        } else {
            l->token = XPATH_TK_DOT;
        }
        break;
    case ':':
        xpath_next_char(l);
        if (l->cur == ':') {
            l->token = XPATH_TK_COLONCOLON;
            xpath_next_char(l);
        } else {
            l->token = XPATH_TK_UNKNOWN;
        }
        break;
    case '*':
        l->token = XPATH_TK_ASTA;
        xpath_next_char(l);
        xpath_check_operator(l, 1);
        break;
    case '/':
        xpath_next_char(l);
        if (l->cur == '/') {
            l->token = XPATH_TK_SLASHSLASH;
            xpath_next_char(l);
        } else {
            l->token = XPATH_TK_SLASH;
        }
        break;
    case '|':
        l->token = XPATH_TK_UNION;
        xpath_next_char(l);
        break;
    case '+':
        l->token = XPATH_TK_ADD;
        xpath_next_char(l);
        break;
    case '-':
        l->token = XPATH_TK_SUB;
        xpath_next_char(l);
        break;
    case '=':
        l->token = XPATH_TK_EQ;
        xpath_next_char(l);
        break;
    case '!':
        xpath_next_char(l);
        if (l->cur == '=') {
            l->token = XPATH_TK_NE;
            xpath_next_char(l);
        } else {
            l->token = XPATH_TK_UNKNOWN;
        }
        break;
    case '<':
        xpath_next_char(l);
        if (l->cur == '=') {
            l->token = XPATH_TK_LE;
            xpath_next_char(l);
        } else {
            l->token = XPATH_TK_LT;
        }
        break;
    case '>':
        xpath_next_char(l);
        if (l->cur == '=') {
            l->token = XPATH_TK_GE;
            xpath_next_char(l);
        } else {
            l->token = XPATH_TK_GT;
        }
        break;
    case '"':
    case '\'':
        l->token = XPATH_TK_STR;
        if (!xpath_get_string(ctx, l)) {
            return;
        }
        break;
    case '0':
CASE_0:;
    case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
        l->token = xpath_get_number(ctx, l);
        if (!l->token) {
            return;
        }
        break;
    default:
        xpath_get_ncname(ctx, l, l->name);
        if (l->name->len > 0) {
            l->token = XPATH_TK_NAME;
            str_clear(l->prefix);
            l->function = 0;
            l->axis = XPATH_AXIS_UNKNOWN;
            int colon2 = 0;
            int savedidx = l->curidx;
            if (l->cur == ':') {
                xpath_next_char(l);
                if (l->cur == ':') {
                    xpath_next_char(l);
                    colon2 = 1;
                    xpath_set_index(l, savedidx);
                } else {
                    xpath_get_ncname(ctx, l, l->prefix);
                    if (l->prefix->len > 0) {
                        char *n = alloca(l->prefix->len + 1);
                        strcpy(n, l->prefix->hd);
                        str_set_cp(ctx, l->prefix, l->name->hd);
                        str_set_cp(ctx, l->name, n);
                        savedidx = l->curidx;
                        xpah_skip_whitespace(l);
                        l->function = (l->cur == '(');
                        xpath_set_index(l, savedidx);
                    } else if (l->cur == '*') {
                        xpath_next_char(l);
                        str_set_cp(ctx, l->prefix, l->name->hd);
                        str_set_cp(ctx, l->name, "*");
                    } else {
                        xpath_set_index(l, savedidx);
                    }
                }
            } else {
                xpah_skip_whitespace(l);
                if (l->cur == ':') {
                    xpath_next_char(l);
                    if (l->cur == ':') {
                        xpath_next_char(l);
                        colon2 = 1;
                    }
                    xpath_set_index(l, savedidx);
                } else {
                    l->function = (l->cur == '(');
                }
            }
            if (!xpath_check_operator(l, 0) && colon2) {
                l->axis = xpath_check_axis(l);
            }
        } else {
            l->token = XPATH_TK_UNKNOWN;
            xpath_next_char(l);
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

static int xpath_throw_runtime_exception(int line, vmctx *ctx, const char *msg)
{
    printf("XPath error at %d\n", line);
    return throw_system_exception(__LINE__, ctx, EXCEPT_XML_ERROR, msg);
}

static vmvar *xpath_make_predicate(vmctx *ctx, vmvar *node, vmvar *condition, int reverse)
{
    vmobj *n = alcobj(ctx);
    n->value = XPATH_OP_PREDICATE;
    KL_SET_PROPERTY_I(n, isPredicate, 1);
    array_push(ctx, n, node);
    array_push(ctx, n, condition);
    array_push(ctx, n, alcvar_int64(ctx, reverse, 0));
    return alcvar_obj(ctx, n);
}

static vmvar *xpath_make_axis(vmctx *ctx, int axis, int nodetype, vmvar *prefix, vmvar *name)
{
    vmobj *n = alcobj(ctx);
    n->value = XPATH_OP_AXIS;
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
    n->value = XPATH_OP_FUNCCALL;
    KL_SET_PROPERTY_I(n, isFunctionCall, 1);
    array_push(ctx, n, prefix);
    array_push(ctx, n, name);
    array_push(ctx, n, args ? alcvar_obj(ctx, args) : NULL);
    return alcvar_obj(ctx, n);
}

static vmvar *xpath_make_binop(vmctx *ctx, int op, vmvar *lhs, vmvar *rhs)
{
    vmobj *n = alcobj(ctx);
    n->value = XPATH_OP_EXPR;
    KL_SET_PROPERTY_I(n, isExpr, 1);
    array_push(ctx, n, alcvar_int64(ctx, op, 0));
    array_push(ctx, n, lhs);
    array_push(ctx, n, rhs);
    return alcvar_obj(ctx, n);
}

static vmvar *xpath_make_step(vmctx *ctx, vmvar *lhs, vmvar *rhs)
{
    vmobj *n = alcobj(ctx);
    n->value = XPATH_OP_STEP;
    KL_SET_PROPERTY_I(n, isStep, 1);
    array_push(ctx, n, lhs);
    array_push(ctx, n, rhs);
    return alcvar_obj(ctx, n);
}

static int xpath_is_node_type(xpath_lexer *l)
{
    return l->prefix->len == 0 && (
        strcmp(l->name->hd, "node") == 0 || strcmp(l->name->hd, "text") == 0 ||
        strcmp(l->name->hd, "processing-instruction") == 0 || strcmp(l->name->hd, "comment") == 0
    );
}

static int xpath_is_step(int token)
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

static int xpath_is_primary_expr(xpath_lexer *l)
{
    return
        l->token == XPATH_TK_STR ||
        l->token == XPATH_TK_INT ||
        l->token == XPATH_TK_NUM ||
        l->token == XPATH_TK_LP ||
        (l->token == XPATH_TK_NAME && l->function && !xpath_is_node_type(l))
    ;
}

static int xpath_is_reverse_axis(int axis)
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

static void xpath_push_posinfo(xpath_lexer *l, int startch, int endch)
{
    l->posinfo[l->stksz++] = startch;
    l->posinfo[l->stksz++] = endch;
}

static void xpath_pop_posinfo(xpath_lexer *l)
{
    l->stksz -= 2;
}

static int xpath_check_node_test(vmctx *ctx, xpath_lexer *l, int axis, int *node_type, vmstr *node_prefix, vmstr *node_name)
{
    switch (l->token) {
    case XPATH_TK_NAME:
        if (l->function && xpath_is_node_type(l)) {
            str_clear(node_prefix);
            str_clear(node_name);
            if      (strcmp(l->name->hd, "comment") == 0) *node_type = XPATH_NODE_COMMENT;
            else if (strcmp(l->name->hd, "text")    == 0) *node_type = XPATH_NODE_TEXT;
            else if (strcmp(l->name->hd, "node")    == 0) *node_type = XPATH_NODE_ALL;
            else                                          *node_type = XPATH_NODE_PROCESSIGN_INSTRUCTION;
            xpath_next_token(ctx, l);
            if (l->token != XPATH_TK_LP) {
                xpath_set_syntax_error(__LINE__, ctx, l);
                return 0;
            }
            xpath_next_token(ctx, l);

            if (*node_type == XPATH_NODE_PROCESSIGN_INSTRUCTION) {
                if (l->token != XPATH_TK_RP) {
                    if (l->token != XPATH_TK_STR) {
                        xpath_set_syntax_error(__LINE__, ctx, l);
                        return 0;
                    }
                    str_clear(node_prefix);
                    str_set_cp(ctx, node_name, l->str->hd);
                    xpath_next_token(ctx, l);
                }
            }

            if (l->token != XPATH_TK_RP) {
                xpath_set_syntax_error(__LINE__, ctx, l);
                return 0;
            }
            xpath_next_token(ctx, l);
        } else {
            str_set_cp(ctx, node_prefix, l->prefix->hd);
            str_set_cp(ctx, node_name, l->name->hd);
            *node_type = xpath_make_node_type(axis);
            xpath_next_token(ctx, l);
            if (strcmp(node_name->hd, "*") == 0) {
                str_clear(node_name);
            }
        }
        break;
    case XPATH_TK_ASTA:
        str_clear(node_prefix);
        str_clear(node_name);
        *node_type = xpath_make_node_type(axis);
        xpath_next_token(ctx, l);
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
        xpath_push_posinfo(l, startch, l->prevend);
        n = xpath_make_axis(ctx, axis, node_type, alcvar_str(ctx, node_prefix->hd), alcvar_str(ctx, node_name->hd));
        xpath_pop_posinfo(l);
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
        xpath_next_token(ctx, l);
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

        xpath_next_token(ctx, l);
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
    xpath_next_token(ctx, l);
    vmvar *n = xpath_expr(ctx, l);
    if (!n) return NULL;
    if (l->token != XPATH_TK_RB) {
        xpath_set_syntax_error(__LINE__, ctx, l);
        return NULL;
    }
    xpath_next_token(ctx, l);
    return n;
}

static vmvar *xpath_step(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = NULL;
    if (l->token == XPATH_TK_DOT) {
        xpath_next_token(ctx, l);
        n = xpath_make_axis(ctx, XPATH_AXIS_SELF, XPATH_NODE_ALL, NULL, NULL);
    } else if (l->token == XPATH_TK_DOTDOT) {
        xpath_next_token(ctx, l);
        n = xpath_make_axis(ctx, XPATH_AXIS_PARENT, XPATH_NODE_ALL, NULL, NULL);
    } else {
        int axis;
        switch (l->token) {
        case XPATH_TK_AXISNAME:
            axis = l->axis;
            xpath_next_token(ctx, l);
            xpath_next_token(ctx, l);
            break;
        case XPATH_TK_AT:
            axis = XPATH_AXIS_ATTRIBUTE;
            xpath_next_token(ctx, l);
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
            n = xpath_make_predicate(ctx, n, n2, xpath_is_reverse_axis(axis));
        }
    }

    return n;
}

static vmvar *xpath_relative_location_path(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = xpath_step(ctx, l);
    if (!n) return NULL;
    if (l->token == XPATH_TK_SLASH) {
        xpath_next_token(ctx, l);
        vmvar *n2 = xpath_relative_location_path(ctx, l);
        if (!n2) return NULL;
        n = xpath_make_step(ctx, n, n2);
    } else if (l->token == XPATH_TK_SLASHSLASH) {
        xpath_next_token(ctx, l);
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
        xpath_next_token(ctx, l);
        n = xpath_make_axis(ctx, XPATH_AXIS_ROOT, XPATH_NODE_ALL, NULL, NULL);
        if (xpath_is_step(l->token)) {
            vmvar *n2 = xpath_relative_location_path(ctx, l);
            if (!n2) return NULL;
            n = xpath_make_step(ctx, n, n2);
        }
    } else if (l->token == XPATH_TK_SLASHSLASH) {
        xpath_next_token(ctx, l);
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
    xpath_next_token(ctx, l);
    if (l->token != XPATH_TK_LP) {
        xpath_set_syntax_error(__LINE__, ctx, l);
        return NULL;
    }
    xpath_next_token(ctx, l);

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
            xpath_next_token(ctx, l);
        }
    }

    xpath_next_token(ctx, l);
    xpath_push_posinfo(l, startch, l->prevend);
    vmvar *n = xpath_make_function(ctx, prefix, name, args);
    xpath_pop_posinfo(l);
    return n;
}

static vmvar *xpath_primary_expr(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = NULL;
    switch (l->token) {
    case XPATH_TK_STR:
        n = alcvar_str(ctx, l->str->hd);
        xpath_next_token(ctx, l);
        break;
    case XPATH_TK_INT: {
        int64_t i = strtoll(l->num->hd, NULL, 10);
        n = alcvar_int64(ctx, i, 0);
        xpath_next_token(ctx, l);
        break;
    }
    case XPATH_TK_NUM: {
        double d = strtod(l->num->hd, NULL);
        n = alcvar_double(ctx, &d);
        xpath_next_token(ctx, l);
        break;
    }
    case XPATH_TK_LP:
        xpath_next_token(ctx, l);
        n = xpath_expr(ctx, l);
        if (l->token != XPATH_TK_RP) {
            xpath_set_syntax_error(__LINE__, ctx, l);
            return NULL;
        }
        xpath_next_token(ctx, l);
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
        xpath_push_posinfo(l, startch, endch);
        vmvar *n2 = xpath_predicate(ctx, l);
        if (!n2) return NULL;
        n = xpath_make_predicate(ctx, n, n2, 0);
        xpath_pop_posinfo(l);
    }

    return n;
}

static vmvar *xpath_path_expr(vmctx *ctx, xpath_lexer *l)
{
    vmvar *n = NULL;
    if (xpath_is_primary_expr(l)) {
        int startch = l->start;
        n = xpath_filter_expr(ctx, l);
        if (!n) return NULL;
        int endch = l->prevend;

        if (l->token == XPATH_TK_SLASH) {
            xpath_next_token(ctx, l);
            xpath_push_posinfo(l, startch, endch);
            vmvar *n2 = xpath_relative_location_path(ctx, l);
            if (!n2) return NULL;
            n = xpath_make_step(ctx, n, n2);
            xpath_pop_posinfo(l);
        } else if (l->token == XPATH_TK_SLASHSLASH) {
            xpath_next_token(ctx, l);
            xpath_push_posinfo(l, startch, endch);
            vmvar *n2 = xpath_relative_location_path(ctx, l);
            if (!n2) return NULL;
            n = xpath_make_step(ctx, n,
                xpath_make_step(ctx,
                    xpath_make_axis(ctx, XPATH_AXIS_DESCENDANT_OR_SELF, XPATH_NODE_ALL, NULL, NULL), n2
                )
            );
            xpath_pop_posinfo(l);
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
        xpath_next_token(ctx, l);
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
    xpath_next_char(&lexer);
    xpath_next_token(ctx, &lexer);
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

/* evaluate XPath */

static int xpath_evaluate_step(vmctx *ctx, vmvar *r, vmvar *xpath, vmvar *root, vmobj *info, vmobj *base);
static int xpath_collect_descendant(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res);

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
            printf("<%d> XML node (%s[%d])", (int)node->o->value, value ? value : "unknown", i + 1);
        } else {
            print_obj(ctx, node);
        }
        printf("\n");
    }
}

static int xpath_comp_forward(const void * n1, const void * n2)
{
    int v1 = (*(vmvar **)n1)->o->value;
    int v2 = (*(vmvar **)n2)->o->value;
    return v1 < v2 ? -1 : (v1 > v2 ? 1 : 0);
}

static int xpath_comp_reverse(const void * n1, const void * n2)
{
    int v1 = (*(vmvar **)n1)->o->value;
    int v2 = (*(vmvar **)n2)->o->value;
    return v1 < v2 ? 1 : (v1 > v2 ? -1 : 0);
}

static void xpath_sort_by_doc_order(vmobj* res)
{
    qsort(res->ary, res->idxsz, sizeof(vmvar *), xpath_comp_forward);
}

static void xpath_sort_by_doc_reverse_order(vmobj* res)
{
    qsort(res->ary, res->idxsz, sizeof(vmvar *), xpath_comp_reverse);
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
        str_append_cp(ctx, qname, ":");
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
    if (!nodet || nodet->t != VAR_INT64) {
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
    if (node->value > 0) {
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
}

static int xpath_collect_attribute(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
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
        return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
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
        return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
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
        return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
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
        return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
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
        return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
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
        return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
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

static void xpath_collect_descendant_node(vmctx *ctx, vmvar *xpath, vmobj *node, vmobj *res)
{
    xpath_collect_child_nodes(ctx, xpath, node, res);
    vmvar *children = hashmap_search(node, "children");
    if (children->t == VAR_OBJ) {
        xpath_collect_descendant(ctx, xpath, children->o, res);
    }
}

static int xpath_collect_descendant(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
    }

    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv && nv->t != VAR_OBJ) continue;
        xpath_collect_descendant_node(ctx, xpath, nv->o, res);
    }

    return 0;
}

static int xpath_collect_descendant_or_self(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
    }

    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv && nv->t != VAR_OBJ) continue;
        xpath_add_item_if_matched(ctx, res, nv, xo);
        xpath_collect_descendant_node(ctx, xpath, nv->o, res);
    }

    return 0;
}

static int xpath_collect_sibling_node(vmctx *ctx, vmvar *xpath, vmobj *node, vmobj *res, const char *direction, int children)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
    }

    int e = 0;
    if (node) {
        vmobj *xo = xpath->o;
        vmvar *next = hashmap_search(node, direction);
        while (next && next->t == VAR_OBJ) {
            xpath_add_item_if_matched(ctx, res, next, xo);
            if (children) {
                xpath_collect_descendant_node(ctx, xpath, next->o, res);
            }
            next = hashmap_search(next->o, direction);
        }
    }

    return e;
}

static int xpath_collect_following_sibling(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    int e = 0;
    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv && nv->t != VAR_OBJ) continue;
        e = xpath_collect_sibling_node(ctx, xpath, nv->o, res, "nextSibling", 0);
    }

    return e;
}

static int xpath_collect_following(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    int e = 0;
    vmobj *xo = xpath->o;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *parent = nodes->ary[i];
        while (parent && parent->t == VAR_OBJ) {
            e = xpath_collect_sibling_node(ctx, xpath, parent->o, res, "nextSibling", 1);
            if (e != 0) break;
            parent = hashmap_search(parent->o, "parentNode");
        }
    }

    return e;
}

static int xpath_collect_preceding_sibling(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    int e = 0;
    int len = nodes->idxsz;
    for (int i = 0; i < len; ++i) {
        vmvar *nv = nodes->ary[i];
        if (nv && nv->t != VAR_OBJ) continue;
        e = xpath_collect_sibling_node(ctx, xpath, nv->o, res, "previousSibling", 0);
    }

    return e;
}

static int xpath_collect_preceding(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    int e = 0;
    int len = nodes->idxsz;
    for (int i = 0; i < len && e == 0; ++i) {
        vmvar *parent = nodes->ary[i];
        while (parent && parent->t == VAR_OBJ) {
            e = xpath_collect_sibling_node(ctx, xpath, parent->o, res, "previousSibling", 1);
            if (e != 0) break;
            parent = hashmap_search(parent->o, "parentNode");
        }
    }

    return e;
}

static int xpath_collect_self(vmctx *ctx, vmvar *xpath, vmobj *nodes, vmobj *res)
{
    if (xpath->t != VAR_OBJ) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
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
        xpath_sort_by_doc_reverse_order(res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_ANCESTOR_OR_SELF:
        e = xpath_collect_ancestor_or_self(ctx, xpath, info, res);
        xpath_sort_by_doc_reverse_order(res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_ATTRIBUTE:
        e = xpath_collect_attribute(ctx, xpath, info, res);
        SET_OBJ(r, res);
        break;
    case XPATH_AXIS_CHILD:
        e = xpath_collect_child(ctx, xpath, info, res);
        xpath_sort_by_doc_order(res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_DESCENDANT:
        e = xpath_collect_descendant(ctx, xpath, info, res);
        xpath_sort_by_doc_order(res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_DESCENDANT_OR_SELF:
        e = xpath_collect_descendant_or_self(ctx, xpath, info, res);
        xpath_sort_by_doc_order(res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_FOLLOWING:
        e = xpath_collect_following(ctx, xpath, info, res);
        xpath_sort_by_doc_order(res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_FOLLOWING_SIBLING:
        e = xpath_collect_following_sibling(ctx, xpath, info, res);
        xpath_sort_by_doc_order(res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_NAMESPACE:
        /* TODO */
        break;
    case XPATH_AXIS_PARENT:
        e = xpath_collect_parent(ctx, xpath, info, res);
        xpath_sort_by_doc_reverse_order(res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_PRECEDING:
        e = xpath_collect_preceding(ctx, xpath, info, res);
        xpath_sort_by_doc_reverse_order(res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_PRECEDING_SIBLING:
        e = xpath_collect_preceding_sibling(ctx, xpath, info, res);
        xpath_sort_by_doc_reverse_order(res);
        SET_OBJ(r, res);
        xpath_reset_collected_nodes(ctx, res);
        break;
    case XPATH_AXIS_SELF:
        e = xpath_collect_self(ctx, xpath, info, res);
        xpath_sort_by_doc_order(res);
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
    switch (v->t) {
    case VAR_BOOL: {
        v->t = VAR_STR;
        v->s = alcstr_str(ctx, v->i ? "true" : "false");
        break;
    }
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
    case VAR_STR:
        break;
    case VAR_OBJ: {
        vmstr *text = alcstr_str(ctx, "");
        xmldom_get_text_content(ctx, text, v);
        v->t = VAR_STR;
        v->s = text;
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
        xpath_to_string(ctx, lhs);
        xpath_to_string(ctx, rhs);
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

static int xpath_try_do_step(vmctx *ctx, vmvar *r, vmobj **nodes, vmobj *node, vmvar *arg, vmvar *root, vmobj *base)
{
    if (!*nodes) {
        *nodes = alcobj(ctx);
        array_set(ctx, *nodes, 0, alcvar_obj(ctx, node));
    }
    return xpath_evaluate_step(ctx, r, arg, root, *nodes, base);
}

static int xpath_make_str_by_step(vmctx *ctx, vmvar *r, vmobj **nodes, vmobj *node, vmvar **a0, vmvar *root, vmobj *base)
{
    int e = 0;
    vmvar *arg0 = *a0;
    if (arg0->t == VAR_OBJ) {
        e = xpath_try_do_step(ctx, r, nodes, node, arg0, root, base);
        if (e != 0) return e;
        XPATH_EXPAND_V(r);
        *a0 = r;
    }
    if ((*a0)->t != VAR_STR) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Argument's type mismatch");
    }
    return e;
}

static int xpath_try_to_conv_2args(vmctx *ctx, vmvar *r, vmobj *node, vmvar **a0, vmvar **a1, vmvar *root, vmobj *base)
{
    int e = 0;
    vmobj *nodes = NULL;
    e = xpath_make_str_by_step(ctx, r, &nodes, node, a0, root, base);
    if (e == 0) {
        e = xpath_make_str_by_step(ctx, r, &nodes, node, a1, root, base);
    }
    return e;
}

static int xpath_try_to_conv_3args(vmctx *ctx, vmvar *r, vmobj *node, vmvar **a0, vmvar **a1, vmvar **a2, vmvar *root, vmobj *base)
{
    int e = 0;
    vmobj *nodes = NULL;
    e = xpath_make_str_by_step(ctx, r, &nodes, node, a0, root, base);
    if (e == 0) {
        e = xpath_make_str_by_step(ctx, r, &nodes, node, a1, root, base);
        if (e == 0) {
            e = xpath_make_str_by_step(ctx, r, &nodes, node, a2, root, base);
        }
    }
    return e;
}

static int xpath_is_node_set(vmobj *nodes)
{
    for (int i = 0, len = nodes->idxsz; i < len; ++i) {
        if (nodes->ary[i]->t != VAR_OBJ) {
            return 0;
        }
    }
    return 1;
}

static int xpath_get_first_node_in_nodeset(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz < 1) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Too few arguments");
    }
    int e = 0;
    vmobj *nodes = NULL;
    vmvar *arg0 = args->ary[0];
    if (arg0->t == VAR_OBJ) {
        e = xpath_try_do_step(ctx, r, &nodes, node, arg0, root, base);
        if (e != 0) return e;
        arg0 = r;
    }
    if (arg0->t == VAR_OBJ) {
        vmobj *ao = arg0->o;
        if (ao->idxsz <= 0) {
            vmstr *s = alcstr_str(ctx, "");
            SET_SV(r, s);
            return 0;
        } else if (ao->idxsz > 1) {
            if (xpath_is_node_set(ao)) {
                /* Use the first one in the document order. */
                xpath_sort_by_doc_order(ao);
            }
        }
        arg0 = ao->ary[0];
    }
    SHCOPY_VAR_TO(ctx, r, arg0);
    return 0;
}

static int xpath_function_local_name(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    int e = 0;
    const char *ln = NULL;
    if (args->idxsz == 0) {
        ln = hashmap_getstr(node, "localName");
    } else {
        e = xpath_get_first_node_in_nodeset(ctx, r, args, root, node, base);
        if (e != 0) return e;
        if (r->t == VAR_OBJ) {
            ln = hashmap_getstr(r->o, "localName");
        }
    }
    SET_STR(r, ln ? ln : "");
    return e;
}

static int xpath_function_namespace_uri(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    int e = 0;
    const char *ns = NULL;
    if (args->idxsz == 0) {
        ns = hashmap_getstr(node, "namespaceURI");
    } else {
        e = xpath_get_first_node_in_nodeset(ctx, r, args, root, node, base);
        if (e != 0) return e;
        if (r->t == VAR_OBJ) {
            ns = hashmap_getstr(r->o, "namespaceURI");
        }
    }
    SET_STR(r, ns ? ns : "");
    return e;
}

static int xpath_function_name(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    int e = 0;
    const char *nm = NULL;
    if (args->idxsz == 0) {
        nm = hashmap_getstr(node, "qName");
    } else {
        e = xpath_get_first_node_in_nodeset(ctx, r, args, root, node, base);
        if (e != 0) return e;
        if (r->t == VAR_OBJ) {
            nm = hashmap_getstr(r->o, "qName");
        }
    }
    SET_STR(r, nm ? nm : "");
    return e;
}

static int xpath_function_string_core(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    int e = 0;
    e = xpath_get_first_node_in_nodeset(ctx, r, args, root, node, base);
    if (e == 0) {
        xpath_to_string(ctx, r);
    }
    return e;
}

static int xpath_function_string(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    int e = 0;
    if (args->idxsz == 0) {
        SET_OBJ(r, node);
        xpath_to_string(ctx, r);
    } else {
        e = xpath_function_string_core(ctx, r, args, root, node, base);
    }
    return e;
}

static int xpath_function_concat(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz < 2) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Too few arguments");
    }
    vmvar *arg0 = args->ary[0];
    vmvar *arg1 = args->ary[1];
    int e = xpath_try_to_conv_2args(ctx, r, node, &arg0, &arg1, root, base);
    if (e != 0) return e;
    vmstr *s = alcstr_str(ctx, arg0->s->hd);
    str_append_str(ctx, s, arg1->s);
    SET_SV(r, s);
    return 0;
}

static int xpath_function_starts_with(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz < 2) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Too few arguments");
    }
    vmvar *arg0 = args->ary[0];
    vmvar *arg1 = args->ary[1];
    int e = xpath_try_to_conv_2args(ctx, r, node, &arg0, &arg1, root, base);
    if (e != 0) return e;
    const char *s = arg0->s->hd;
    SET_BOOL(r, strstr(s, arg1->s->hd) == s);
    return 0;
}

static int xpath_function_contains(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz < 2) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Too few arguments");
    }
    vmvar *arg0 = args->ary[0];
    vmvar *arg1 = args->ary[1];
    int e = xpath_try_to_conv_2args(ctx, r, node, &arg0, &arg1, root, base);
    if (e != 0) return e;
    SET_BOOL(r, strstr(arg0->s->hd, arg1->s->hd) != NULL);
    return 0;
}

static int xpath_function_substring_before(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz < 2) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Too few arguments");
    }
    vmvar *arg0 = args->ary[0];
    vmvar *arg1 = args->ary[1];
    int e = xpath_try_to_conv_2args(ctx, r, node, &arg0, &arg1, root, base);
    if (e != 0) return e;
    const char *s0 = arg0->s->hd;
    const char *s1 = arg1->s->hd;
    if (*s1 == 0) {
        SET_STR(r, "");
        return 0;
    }
    const char *p = strstr(s0, s1);
    if (p && s0 < p) {
        int len = p - s0;
        char *ss = alloca(len + 1);
        strncpy(ss, s0, len);
        ss[len] = 0;
        SET_STR(r, ss);
    } else {
        SET_STR(r, "");
    }
    return 0;
}

static int xpath_function_substring_after(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz < 2) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Too few arguments");
    }
    vmvar *arg0 = args->ary[0];
    vmvar *arg1 = args->ary[1];
    int e = xpath_try_to_conv_2args(ctx, r, node, &arg0, &arg1, root, base);
    if (e != 0) return e;
    const char *s0 = arg0->s->hd;
    const char *s1 = arg1->s->hd;
    if (*s1 == 0) {
        SET_STR(r, s0);
        return 0;
    }
    const char *p = strstr(s0, s1);
    if (p && s0 < p) {
        SET_STR(r, p + strlen(s1));
    } else {
        SET_STR(r, "");
    }
    return 0;
}

static int xpath_function_substring(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz < 2) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Too few arguments");
    }
    vmvar *arg0 = args->ary[0];
    vmvar *arg1 = args->ary[1];
    if (arg1->t != VAR_INT64 && arg1->t != VAR_DBL) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Argument's type mismatch");
    }
    vmvar *arg2 = args->idxsz > 2 ? args->ary[2] : NULL;
    if (arg2 && arg2->t != VAR_INT64 && arg2->t != VAR_DBL) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Argument's type mismatch");
    }
    vmobj *nodes = NULL;
    int e = xpath_make_str_by_step(ctx, r, &nodes, node, &arg0, root, base);
    if (e != 0) return e;
    const char *s0 = arg0->s->hd;
    int start = (arg1->t == VAR_INT64 ? arg1->i : (int)(arg1->d + 0.5)) - 1;
    if (arg2) {
        int len = arg2->t == VAR_INT64 ? arg2->i : (int)(arg2->d + 0.5);
        if (len < 0) len = 0;
        if (start < 0) {
            len += start;
            start = 0;
        }
        int slen = strlen(s0);
        if (slen <= start) {
            SET_STR(r, "");
        } else {
            if (slen <= (start + len)) {
                len = slen - start;
            }
            vmstr *sv = alcstr_str_len(ctx, s0 + start, len);
            SET_SV(r, sv);
        }
    } else {
        int slen = strlen(s0);
        if (slen <= start) {
            SET_STR(r, "");
        } else {
            SET_STR(r, s0 + start);
        }
    }
    return 0;
}

static int xpath_function_string_length(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    int e = 0;
    if (args->idxsz == 0) {
        SET_OBJ(r, node);
    } else {
        e = xpath_get_first_node_in_nodeset(ctx, r, args, root, node, base);
        if (e != 0) return e;
    }
    xpath_to_string(ctx, r);
    int len = r->s->len;
    SET_I64(r, len);
    return e;
}

static int xpath_function_normalize_space(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    int e = 0;
    if (args->idxsz == 0) {
        SET_OBJ(r, node);
    } else {
        e = xpath_get_first_node_in_nodeset(ctx, r, args, root, node, base);
        if (e != 0) return e;
    }
    xpath_to_string(ctx, r);
    int ws = 0;
    char *s = alloca(r->s->len);
    char *sp = s;
    const char *p = r->s->hd;
    while (is_whitespace(*p)) ++p;
    for ( ; *p; ++p) {
        if (is_whitespace(*p)) {
            ws = 1;
            continue;
        }
        if (ws) {
            *s++ = ' ';
            ws = 0;
        }
        *s++ = *p;
    }
    *s = 0;
    SET_STR(r, sp);
    return e;
}

static int xpath_search_string_position(unsigned char c, const char *s)
{
    for (int i = 0; *s; ++s, ++i) {
        if (*s == c) {
            return i + 1;
        }
    }
    return -1;
}

static int xpath_function_translate(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz < 3) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Too few arguments");
    }
    vmvar *arg0 = args->ary[0];
    vmvar *arg1 = args->ary[1];
    vmvar *arg2 = args->ary[2];
    int e = xpath_try_to_conv_3args(ctx, r, node, &arg0, &arg1, &arg2, root, base);
    if (e != 0) return e;
    const char *s0 = arg0->s->hd;
    const char *s1 = arg1->s->hd;
    const char *s2 = arg2->s->hd;
    int len2 = arg2->s->len;

    char table[256] = {0};
    char *s = alloca(arg0->s->len + 1);
    char *sp = s;
    for ( ; *s0; ++s0) {
        unsigned char c = *s0;
        if (table[c] == 0) {
            table[c] = xpath_search_string_position(c, s1);
        }
        int pos = table[c] - 1;
        if (pos < 0) {
            *s++ = *s0;
        } else if (pos < len2) {
            *s++ = s2[pos];
        }
    }
    *s = 0;
    SET_STR(r, sp);
    return e;
}

static int xpath_function_boolean_core(vmctx *ctx, vmvar *r, vmvar *arg0, vmvar *root, vmobj *node, vmobj *base)
{
    int e = 0;
    vmobj *nodes = NULL;
    if (arg0->t == VAR_OBJ) {
        e = xpath_try_do_step(ctx, r, &nodes, node, arg0, root, base);
        if (e != 0) return e;
        arg0 = r;
    }
    if (arg0->t == VAR_OBJ) {
        vmobj *ao = arg0->o;
        if (ao->idxsz > 1 && !xpath_is_node_set(ao)) {
            arg0 = ao->ary[0];
        }
    }

    SHCOPY_VAR_TO(ctx, r, arg0);
    switch (r->t) {
    case VAR_BOOL:
        break;
    case VAR_INT64:
        r->t = VAR_BOOL;
        break;
    case VAR_DBL:
        r->t = VAR_BOOL;
        r->i = r->d >= DBL_EPSILON;
        break;
    case VAR_BIG:
        r->t = VAR_BOOL;
        r->i = BzToDouble(r->bi->b) >= DBL_EPSILON;
        break;
    case VAR_STR:
        r->t = VAR_BOOL;
        r->i = r->s->len > 0;
        break;
    case VAR_OBJ:
        r->t = VAR_BOOL;
        r->i = r->o->idxsz > 0;
        break;
    default:
        r->t = VAR_BOOL;
        r->i = 0;
        break;
    }
    return e;
}

static int xpath_function_boolean(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz < 1) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Too few arguments");
    }
    vmvar *arg0 = args->ary[0];
    xpath_function_boolean_core(ctx, r, arg0, root, node, base);
    return 0;
}

static int xpath_function_not(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz < 1) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Too few arguments");
    }
    vmvar *arg0 = args->ary[0];
    xpath_function_boolean_core(ctx, r, arg0, root, node, base);

    r->i = r->i ? 0 : 1;
    return 0;
}

static int xpath_function_true(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    SET_BOOL(r, 1);
    return 0;
}

static int xpath_function_false(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    SET_BOOL(r, 0);
    return 0;
}

static int xpath_function_number_core(vmctx *ctx, vmvar *r, vmvar *arg0, vmvar *root, vmobj *node, vmobj *base)
{
    int e = 0;
    vmobj *nodes = NULL;
    if (arg0->t == VAR_OBJ) {
        e = xpath_try_do_step(ctx, r, &nodes, node, arg0, root, base);
        if (e != 0) return e;
        arg0 = r;
    }
    if (arg0->t == VAR_OBJ) {
        vmobj *ao = arg0->o;
        if (ao->idxsz > 1 && !xpath_is_node_set(ao)) {
            arg0 = ao->ary[0];
        }
    }
    SHCOPY_VAR_TO(ctx, r, arg0);
    switch (r->t) {
    case VAR_BOOL:
        r->t = VAR_INT64;
        break;
    case VAR_INT64:
    case VAR_DBL:
        break;
    default:
        xpath_to_string(ctx, r);
        xpath_to_number(r);
        break;
    }
    return e;
}

static int xpath_function_number(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz == 0) {
        vmvar n = { .t = VAR_OBJ, .o = node };
        xpath_to_number(&n);
        SHCOPY_VAR_TO(ctx, r, &n);
    } else {
        vmvar *arg0 = args->ary[0];
        xpath_function_number_core(ctx, r, arg0, root, node, base);
    }
    return 0;
}

static int xpath_function_sum(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz < 1) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Too few arguments");
    }
    vmvar *arg0 = args->ary[0];
    int e = 0;
    vmobj *nodes = NULL;
    if (arg0->t == VAR_OBJ) {
        e = xpath_try_do_step(ctx, r, &nodes, node, arg0, root, base);
        if (e != 0) return e;
        arg0 = r;
    }
    if (arg0->t != VAR_OBJ) {
        xpath_to_string(ctx, arg0);
        xpath_to_number(arg0);
    } else {
        vmobj *ao = arg0->o;
        if (ao->idxsz <= 0) {
            SET_I64(r, 0);
        } else if (xpath_is_node_set(ao)) {
            int len = ao->idxsz;
            SET_I64(arg0, 0);
            for (int i = 0; i < len; ++i) {
                vmvar *elem = ao->ary[i];
                xpath_to_string(ctx, elem);
                xpath_to_number(elem);
                OP_ADD(ctx, arg0, arg0, elem, L0, __func__, __FILE__, 0);
            }
        } else {
            arg0 = ao->ary[0];
            xpath_to_string(ctx, arg0);
            xpath_to_number(arg0);
        }
    }

L0:;
    SHCOPY_VAR_TO(ctx, r, arg0);
    return e;
}

static int xpath_function_floor(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz < 1) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Too few arguments");
    }
    vmvar *arg0 = args->ary[0];
    xpath_function_number_core(ctx, r, arg0, root, node, base);
    if (r->t == VAR_DBL) {
        r->t = VAR_INT64;
        r->i = (int64_t)r->d;
    }
    return 0;
}

static int xpath_function_ceiling(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz < 1) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Too few arguments");
    }
    vmvar *arg0 = args->ary[0];
    xpath_function_number_core(ctx, r, arg0, root, node, base);
    if (r->t == VAR_DBL) {
        r->t = VAR_INT64;
        r->i = (int64_t)(r->d + 1);
    }
    return 0;
}

static int xpath_function_round(vmctx *ctx, vmvar *r, vmobj *args, vmvar *root, vmobj *node, vmobj *base)
{
    if (args->idxsz < 1) {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Too few arguments");
    }
    vmvar *arg0 = args->ary[0];
    xpath_function_number_core(ctx, r, arg0, root, node, base);
    if (r->t == VAR_DBL) {
        r->t = VAR_INT64;
        r->i = (int64_t)(r->d + 0.5);
    }
    return 0;
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
    // } else if (strcmp(funcname, "id") == 0) {
    //     e = xpath_function_id(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "local-name") == 0) {
        e = xpath_function_local_name(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "namespace-uri") == 0) {
        e = xpath_function_namespace_uri(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "name") == 0) {
        e = xpath_function_name(ctx, r, args, root, node, base);

    } else if (strcmp(funcname, "string") == 0) {
        e = xpath_function_string(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "concat") == 0) {
        e = xpath_function_concat(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "starts-with") == 0) {
        e = xpath_function_starts_with(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "contains") == 0) {
        e = xpath_function_contains(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "substring-before") == 0) {
        e = xpath_function_substring_before(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "substring-after") == 0) {
        e = xpath_function_substring_after(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "substring") == 0) {
        e = xpath_function_substring(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "string-length") == 0) {
        e = xpath_function_string_length(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "normalize-space") == 0) {
        e = xpath_function_normalize_space(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "translate") == 0) {
        e = xpath_function_translate(ctx, r, args, root, node, base);

    } else if (strcmp(funcname, "boolean") == 0) {
        e = xpath_function_boolean(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "not") == 0) {
        e = xpath_function_not(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "true") == 0) {
        e = xpath_function_true(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "false") == 0) {
        e = xpath_function_false(ctx, r, args, root, node, base);
    // } else if (strcmp(funcname, "lang") == 0) {
    //     e = xpath_function_lang(ctx, r, args, root, node, base);

    } else if (strcmp(funcname, "number") == 0) {
        e = xpath_function_number(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "sum") == 0) {
        e = xpath_function_sum(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "floor") == 0) {
        e = xpath_function_floor(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "ceiling") == 0) {
        e = xpath_function_ceiling(ctx, r, args, root, node, base);
    } else if (strcmp(funcname, "round") == 0) {
        e = xpath_function_round(ctx, r, args, root, node, base);

    } else {
        return xpath_throw_runtime_exception(__LINE__, ctx, "Unsupported function name");
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
        if (r.t == VAR_INT64) {
            int position = i + 1;
            if (0 < position && r.i == position) {
                array_push(ctx, resobj, node);
            }
        } else if (r.t == VAR_BOOL) {
            if (r.i != 0) {
                array_push(ctx, resobj, node);
            }
        } else {
            xpath_to_boolean(&r);
            if (r.i != 0) {
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
        switch (xobj->value) {
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
                return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
            }
            vmvar *xobjv0 = xobj->ary[0];
            int op = xobjv0->t == VAR_INT64 ? xobjv0->i : XPATH_OP_UNKNOWN;
            e = xpath_proc_expr(ctx, op, r, xobj->ary[1], xobj->ary[2], info, root, base);
            break;
        }
        case XPATH_OP_FUNCCALL: {
            XPATH_EXPAND_O(info);
            if (!info) {
                return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
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
            return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
        }
        break;
    }
    default:
        return xpath_throw_runtime_exception(__LINE__, ctx, "Invalid XPath state");
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
    if (n1 && n1->t == VAR_OBJ) {
        vmvar *doc = hashmap_search(n1->o, "ownerDocument");
        if (doc && doc->t == VAR_OBJ) {
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

    vmobj *no = node->o;
    vmvar *root = hashmap_search(no, "documentElement");
    if (root) {
        if (no->value > 0) {
            int i = 0;
            xmldom_update_node_number(root, &i);
        }
        node = root;
        no = node->o;
    } else {
        xmldom_update_node_number_all(node);
    }

    vmvar xpath = {0};
    xpath_compile(ctx, &xpath, str->s->hd);
    vmobj *info = xpath_make_initial_info(ctx, no);
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
