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

#define errset_wuth_ret(err, msg) do { *err = msg; if (0) { printf("[%d] err set %s\n", __LINE__, #msg); } return p; } while (0)
#define is_whitespace(x) ((x) == ' ' || (x) == '\t' || (x) == '\r' || (x) == '\n')
#define skip_whitespace(p) do { while (is_whitespace(*p)) { if (*p == '\n') { ++*line; *pos = -1; } ++p; ++*pos; } } while (0)
#define move_next(p) do { if (*p == '\n') { ++*line, *pos = -1;} ++p; ++*pos; } while (0)
#define check_error(err, condition, code) do { if (condition) { errset_wuth_ret(err, code); } } while (0)
#define syntax_error(err, condition) check_error(err, condition, XMLDOM_ESYNTAX)
#define end_of_text_error(err, p) syntax_error(err, *p == 0)
static const char *parse_doc(vmctx *ctx, vmfrm *lex, vmobj *nsmap, vmvar *r, const char *p, int *err, int *line, int *pos, int depth);

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

static const char *parse_attrs(vmctx *ctx, vmobj *nsmap, vmobj *attrs, const char *p, int *err, int *line, int *pos)
{
    const char *attrn = p;
    while (!is_whitespace(*p)) {
        end_of_text_error(err, p);
        if (*p == '=') break;
        move_next(p);
    }
    int attrnlen = p - attrn;
    skip_whitespace(p);
    syntax_error(err, (*p != '='));
    move_next(p);
    skip_whitespace(p);
    syntax_error(err, (*p != '"' && *p != '\''));
    char br = *p;
    move_next(p);
    const char *attrv = p;
    while (*p != br) {
        end_of_text_error(err, p);
        if (*p == '\\') { move_next(p); }
        move_next(p);
    }
    if (strncmp(attrn, "xmlns", 5) == 0 && (*(attrn + 5) == '=' || is_whitespace(*(attrn+5)))) {
        vmvar *value = make_str_obj(ctx, err, attrv, p - attrv);
        array_set(ctx, nsmap, 0, value);
    } else if (strncmp(attrn, "xmlns:", 6) == 0) {
        set_attrs(ctx, err, nsmap, attrn + 6, attrnlen - 6, attrv, p - attrv);
    } else {
        set_attrs(ctx, err, attrs, attrn, attrnlen, attrv, p - attrv);
    }
    if (*err < 0) return p;
    if (*p == br) { move_next(p); }
    return p;
}

static const char *parse_comment(vmctx *ctx, vmvar *r, const char *p, int *err, int *line, int *pos)
{
    const char *s = p;
    while (*p) {
        move_next(p);
        if (*p == '-' && *(p+1) == '-' && *(p+2) == '>') {
            p += 2;
            break;
        }
    }
    syntax_error(err, (*p != '>'));
    vmvar *comment = make_pure_str_obj(ctx, err, s, p - s - 2);
    hashmap_set(ctx, r->o, "nodeName", alcvar_str(ctx, "#comment"));
    hashmap_set(ctx, r->o, "nodeValue", comment);
    hashmap_set(ctx, r->o, "comment", comment);
    return p;
}

static const char *parse_pi(vmctx *ctx, vmobj *nsmap, vmvar *doc, vmvar *r, const char *p, int *err, int *line, int *pos, int *xmldecl)
{
    const char *s = p;
    while (!is_whitespace(*p)) {
        end_of_text_error(err, p);
        move_next(p);
    }
    if (s != p) {
        vmvar *target = make_str_obj(ctx, err, s, p - s);
        if (*err < 0) return p;
        if (strcmp(target->s->s, "xml") == 0) {
            *xmldecl = 1;
        }
        hashmap_set(ctx, r->o, "target", target);
        hashmap_set(ctx, r->o, "nodeName", target);
    }
    skip_whitespace(p);
    if (*p == '?') {
        move_next(p);
        syntax_error(err, (*p != '>'));
        return p;
    }
    vmobj *attrs = alcobj(ctx);
    while (1) {
        skip_whitespace(p);
        end_of_text_error(err, p);
        if (*p == '?') break;
        p = parse_attrs(ctx, nsmap, attrs, p, err, line, pos);
        if (*err < 0) return p;
    }
    if (*xmldecl) {
        vmvar *version = hashmap_search(attrs, "version");
        if (!version) version = alcvar_str(ctx, "1.0");
        hashmap_set(ctx, doc->o, "xmlVersion", version);
        vmvar *encoding = hashmap_search(attrs, "encoding");
        if (!encoding) encoding = alcvar_str(ctx, "UTF-8");
        hashmap_set(ctx, doc->o, "xmlEncoding", encoding);
        vmvar *standalone = hashmap_search(attrs, "standalone");
        int sv = !standalone || (standalone->t == VAR_STR && strcmp(standalone->s->s, "yes") == 0);
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
        return ns->s->s;
    }
    return "";
}

static const char *parse_node(vmctx *ctx, vmfrm *lex, vmobj *nsmap, vmvar *r, const char *p, int *err, int *line, int *pos, int depth)
{
    skip_whitespace(p);
    end_of_text_error(err, p);

    const char *tag = p;
    int taglen = 0;
    const char *ns = p;
    int nslen = 0;
    while (!is_whitespace(*p)) {
        end_of_text_error(err, p);
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
    syntax_error(err, (taglen == 0));

    KL_SET_PROPERTY_I(r->o, "isXmlNode", 1);
    vmvar *lname = make_str_obj(ctx, err, tag, taglen);
    if (*err < 0) return p;
    hashmap_set(ctx, r->o, "localName", lname);
    vmvar *nsname = NULL;
    if (nslen > 0) {
        nsname = make_str_obj(ctx, err, ns, nslen);
        if (*err < 0) return p;
        hashmap_set(ctx, r->o, "prefix", nsname);
        vmvar *tname = make_str_obj(ctx, err, ns, nslen + taglen + 1);
        if (*err < 0) return p;
        hashmap_set(ctx, r->o, "tagName", tname);
        hashmap_set(ctx, r->o, "nodeName", tname);
    } else {
        hashmap_set(ctx, r->o, "prefix", alcvar_str(ctx, ""));
        hashmap_set(ctx, r->o, "tagName", lname);
        hashmap_set(ctx, r->o, "nodeName", lname);
    }
    skip_whitespace(p);

    vmobj *attrs = alcobj(ctx);
    if (*p != '/' && *p != '>') {
        while (1) {
            skip_whitespace(p);
            end_of_text_error(err, p);
            if (*p == '/' || *p == '>') break;
            p = parse_attrs(ctx, nsmap, attrs, p, err, line, pos);
            if (*err < 0) return p;
        }
    }
    hashmap_set(ctx, r->o, "attributes", alcvar_obj(ctx, attrs));

    if (nsname) {
        const char *uri = get_namespace_uri(ctx, nsmap, nsname->s->s);
        hashmap_set(ctx, r->o, "namespaceURI", alcvar_str(ctx, uri));
    } else {
        const char *uri = get_namespace_uri(ctx, nsmap, NULL);
        hashmap_set(ctx, r->o, "namespaceURI", alcvar_str(ctx, uri));
    }

    if (*p == '/') { move_next(p); return p; }
    if (*p == '>') { move_next(p); }
    p = parse_doc(ctx, lex, nsmap, r, p, err, line, pos, depth + 1);
    if (*err < 0) return p;
    syntax_error(err, (*p != '/'));
    move_next(p);

    const char *ctag = p;
    int ctaglen = 0;
    const char *cns = p;
    int cnslen = 0;
    while (!is_whitespace(*p)) {
        end_of_text_error(err, p);
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
        errset_wuth_ret(err, XMLDOM_ECLOSE);
    }
    if (nslen > 0) {
        if (!is_same_tagname(ns, nslen, cns, cnslen)) {
            errset_wuth_ret(err, XMLDOM_ECLOSE);
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
    if (id && id->t == VAR_STR && strcmp(id->s->s, idvalue) == 0) {
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
    vmvar *found = XmlDom_getElementById_Node(a0, a1->s->s);
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
    if (tagName && tagName->t == VAR_STR && strcmp(tagName->s->s, name) == 0) {
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
    XmlDom_getElementsByTagName_Node(ctx, nodes, a0, a1->s->s);
    SET_OBJ(r, nodes);
    return 0;
}

static void setup_xmlnode_props(vmctx *ctx, vmfrm *lex, vmvar *r, vmvar *parent, int index, int type)
{
    vmobj *o = r->o;
    o->is_sysobj = 1;
    KL_SET_PROPERTY_I(o, nodeType, type);
    KL_SET_PROPERTY_I(o, hasChildNodes, o->idxsz > 0 ? 1 : 0);
    vmobj *children = alcobj(ctx);
    for (int i = 0, n = o->idxsz; i < n; i++) {
        array_push(ctx, children, o->ary[i]);
    }
    KL_SET_PROPERTY(o, children, alcvar_obj(ctx, children));
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
    KL_SET_METHOD(o, getElementById, XmlDom_getElementById, lex, 1);
    KL_SET_METHOD(o, getElementsByTagName, XmlDom_getElementsByTagName, lex, 1);
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
}

static const char *parse_doc(vmctx *ctx, vmfrm *lex, vmobj *nsmap, vmvar *r, const char *p, int *err, int *line, int *pos, int depth)
{
    while (*p != 0) {
        const char *s = p;
        while (*p != '<') {
            if (*p == 0) break;
            move_next(p);
        }
        if (s != p) {
            vmvar *vs = make_str_obj(ctx, err, s, p - s);
            if (*err < 0) return p;
            vmvar *vo = alcvar_obj(ctx, alcobj(ctx));
            KL_SET_PROPERTY(vo->o, text, vs);
            KL_SET_PROPERTY(vo->o, nodeValue, vs);
            KL_SET_PROPERTY_S(vo->o, nodeName, "#text");
            setup_xmlnode_props(ctx, lex, vo, r, r->o->idxsz, XMLDOM_TEXT_NODE);
            array_push(ctx, r->o, vo);
        }
        if (*p == 0) break;
        if (*p == '<') {
            vmobj *cnsmap = object_copy(ctx, nsmap);
            move_next(p);
            if (*p == '/') return p;
            vmvar *n = alcvar_obj(ctx, alcobj(ctx));
            if (*p == '!') {
                syntax_error(err, (*++p != '-'));
                syntax_error(err, (*++p != '-'));
                p = parse_comment(ctx, n, p+1, err, line, pos);
                setup_xmlnode_props(ctx, lex, n, r, r->o->idxsz, XMLDOM_COMMENT_NODE);
            } else if (*p == '?') {
                move_next(p);
                int xmldecl = 0;
                p = parse_pi(ctx, cnsmap, r, n, p, err, line, pos, &xmldecl);
                if (xmldecl && (depth > 0 || r->o->idxsz > 0)) {
                    errset_wuth_ret(err, XMLDOM_EDECL);
                }
                setup_xmlnode_props(ctx, lex, n, r, r->o->idxsz,
                    xmldecl ? XMLDOM_XMLDECL_NODE : XMLDOM_PROCESSING_INSTRUCTION_NODE);
            } else {
                p = parse_node(ctx, lex, cnsmap, n, p, err, line, pos, depth);
                setup_xmlnode_props(ctx, lex, n, r, r->o->idxsz, XMLDOM_ELEMENT_NODE);
            }
            if (*err < 0) return p;
            array_push(ctx, r->o, n);
        }
        if (*p == 0) break;
        syntax_error(err, (*p != '>'));
        move_next(p);
    }
    return p;
}

static int XmlDom_parseString(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    vmstr *s = a0->s;
    const char *str = s->s;
    while (is_whitespace(*str)) {
        ++str;
    }
    vmobj *nsmap = alcobj(ctx);
    r->t = VAR_OBJ;
    r->o = alcobj(ctx);
    r->o->is_sysobj = 1;
    if (s->s) {
        int err = 0;
        int line = 1;
        int pos = 0;
        parse_doc(ctx, lex, nsmap, r, str, &err, &line, &pos, 0);
        setup_xmldoc_props(ctx, lex, r);
        if (err < 0) {
            return xmldom_error(ctx, err, line, pos);
        }
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
