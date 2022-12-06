#ifndef KILITE_AMALGAMATION
#include "common.h"
#include "lib.h"
#endif

/* XmlDom */

enum {
    XMLDOM_EDECL = -4,
    XMLDOM_EREF = -3,
    XMLDOM_ECLOSE = -2,
    XMLDOM_ESYNTAX = -1,
    XMLDOM_OK = 0,
};

enum {
    XMLDOM_XMLDECL_NODE = 1,
    XMLDOM_ATTRIBUTE_NODE,
    XMLDOM_CDATA_SECTION_NODE,
    XMLDOM_COMMENT_NODE,
    XMLDOM_DOCUMENT_FRAGMENT_NODE,
    XMLDOM_DOCUMENT_NODE,
    XMLDOM_DOCUMENT_POSITION_CONTAINED_BY,
    XMLDOM_DOCUMENT_POSITION_CONTAINS,
    XMLDOM_DOCUMENT_POSITION_DISCONNECTED,
    XMLDOM_DOCUMENT_POSITION_FOLLOWING,
    XMLDOM_DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC,
    XMLDOM_DOCUMENT_POSITION_PRECEDING,
    XMLDOM_DOCUMENT_TYPE_NODE,
    XMLDOM_ELEMENT_NODE,
    XMLDOM_ENTITY_NODE,
    XMLDOM_ENTITY_REFERENCE_NODE,
    XMLDOM_NOTATION_NODE,
    XMLDOM_PROCESSING_INSTRUCTION_NODE,
    XMLDOM_TEXT_NODE,
};

#define errset_wuth_ret(err, msg) { *err = msg; /* printf("[%d] err set %s\n", __LINE__, #msg); */ return p; }
#define chk_esyntx(p, err) if (*p == 0) { errset_wuth_ret(err, XMLDOM_ESYNTAX); }
#define is_whitespace(x) ((x) == ' ' || (x) == '\t' || (x) == '\r' || (x) == '\n')
#define skip_whitespace(p) while (is_whitespace(*p)) { if (*p == '\n') { ++*line; *pos = -1; } ++p; ++*pos; }
#define move_next(p) { if (*p == '\n') { ++*line, *pos = -1;} ++p; ++*pos; }
static const char *parse_doc(vmctx *ctx, vmfrm *lex, vmvar *r, const char *p, int *err, int *line, int *pos, int depth);

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

static vmvar *make_str_obj(vmctx *ctx, const char *s, int len)
{
    char *buf = alloca(len + 2);
    strncpy(buf, s, len);
    buf[len] = 0;
    return alcvar_str(ctx, buf);
}

static void set_attrs(vmctx *ctx, vmobj *attrs, const char *n, int nlen, const char *v, int vlen)
{
    vmvar *value = make_str_obj(ctx, v, vlen);
    char *key = alloca(nlen + 2);
    strncpy(key, n, nlen);
    key[nlen] = 0;
    hashmap_set(ctx, attrs, key, value);
}

static const char *patse_attrs(vmctx *ctx, vmobj *attrs, const char *p, int *err, int *line, int *pos)
{
    const char *attrn = p;
    while (!is_whitespace(*p)) {
        chk_esyntx(p, err);
        if (*p == '=') break;
        move_next(p);
    }
    int attrnlen = p - attrn;
    skip_whitespace(p);
    if (*p != '=') {
        errset_wuth_ret(err, XMLDOM_ESYNTAX);
    }
    move_next(p);
    skip_whitespace(p);
    if (*p != '"') {
        errset_wuth_ret(err, XMLDOM_ESYNTAX);
    }
    const char *attrv = p;
    move_next(p);
    while (*p != '"') {
        chk_esyntx(p, err);
        if (*p == '\\') {
            move_next(p);
        }
        move_next(p);
    }
    if (*p == '"') {
        move_next(p);
    }
    set_attrs(ctx, attrs, attrn, attrnlen, attrv, p - attrv);
    return p;
}

static const char *patse_pi(vmctx *ctx, vmvar *r, const char *p, int *err, int *line, int *pos, int *xmldecl)
{
    const char *s = p;
    while (!is_whitespace(*p)) {
        chk_esyntx(p, err);
        move_next(p);
    }
    if (s != p) {
        vmvar *target = make_str_obj(ctx, s, p - s);
        if (strcmp(target->s->s, "xml") == 0) {
            *xmldecl = 1;
        }
        hashmap_set(ctx, r->o, "_$target", target);
        KL_SET_PROPERTY_I(r->o, "isProcessingInstructionNode", 1);
    }
    skip_whitespace(p);
    if (*p == '?') {
        move_next(p);
        if (*p != '>') {
            errset_wuth_ret(err, XMLDOM_ESYNTAX);
        }
        return p;
    }
    vmobj *attrs = alcobj(ctx);
    while (1) {
        skip_whitespace(p);
        chk_esyntx(p, err);
        if (*p == '?') break;
        p = patse_attrs(ctx, attrs, p, err, line, pos);
        if (*err < 0) {
            return p;
        }
    }

    hashmap_set(ctx, r->o, "_$data", alcvar_obj(ctx, attrs));
    move_next(p);
    return p;
}

static const char *patse_node(vmctx *ctx, vmfrm *lex, vmvar *r, const char *p, int *err, int *line, int *pos, int depth)
{
    skip_whitespace(p);
    chk_esyntx(p, err);

    const char *tag = p;
    int taglen = 0;
    const char *ns = p;
    int nslen = 0;
    while (!is_whitespace(*p)) {
        chk_esyntx(p, err);
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
    if (taglen == 0) {
        errset_wuth_ret(err, XMLDOM_ESYNTAX);
    }

    KL_SET_PROPERTY_I(r->o, "isXmlNode", 1);
    vmvar *tagname = make_str_obj(ctx, tag, taglen);
    hashmap_set(ctx, r->o, "name", tagname);
    if (nslen > 0) {
        vmvar *nsname = make_str_obj(ctx, ns, nslen);
        hashmap_set(ctx, r->o, "_$ns", nsname);
    }
    skip_whitespace(p);

    vmobj *attrs = alcobj(ctx);
    if (*p != '/' && *p != '>') {
        while (1) {
            skip_whitespace(p);
            chk_esyntx(p, err);
            if (*p == '/' || *p == '>') break;
            p = patse_attrs(ctx, attrs, p, err, line, pos);
            if (*err < 0) {
                return p;
            }
        }
    }
    hashmap_set(ctx, r->o, "_$attrs", alcvar_obj(ctx, attrs));

    if (*p == '/') {
        move_next(p);
        return p;
    }

    if (*p == '>') {
        move_next(p);
    }
    p = parse_doc(ctx, lex, r, p, err, line, pos, depth + 1);
    if (*err < 0) {
        return p;
    }
    if (*p != '/') {
        errset_wuth_ret(err, XMLDOM_ESYNTAX);
    }
    move_next(p);

    const char *ctag = p;
    int ctaglen = 0;
    const char *cns = p;
    int cnslen = 0;
    while (!is_whitespace(*p)) {
        chk_esyntx(p, err);
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

static int XmlDom_documentElement(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    vmvar **ary = a0->o->ary;
    int n = a0->o->idxsz;
    for (int i = 0; i < n; i++) {
        vmvar *v = hashmap_search(ary[i]->o, "type");
        if (v && v->t == VAR_INT64 && v->i == XMLDOM_ELEMENT_NODE) {
            COPY_VAR_TO(ctx, r, ary[i]);
            return 0;
        }
    }
    SET_UNDEF(r);
    return 0;
}

static int XmlDom_tagName(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    vmvar *v = hashmap_search(a0->o, "name");
    COPY_VAR_TO(ctx, r, v);
    return 0;
}

static int XmlDom_text(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    vmvar *v = hashmap_search(a0->o, "_$text");
    COPY_VAR_TO(ctx, r, v);
    return 0;
}

static int XmlDom_attributes(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    vmvar *v = hashmap_search(a0->o, "_$attrs");
    COPY_VAR_TO(ctx, r, v);
    return 0;
}

static int XmlDom_hasChildNodes(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    SET_I64(r, (a0->o->idxsz > 0 ? 1 : 0))
    return 0;
}

static int XmlDom_parentNode(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    vmvar *v = hashmap_search(a0->o, "_$parent");
    COPY_VAR_TO(ctx, r, v);
    return 0;
}

static int XmlDom_childNodes(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    r->t = VAR_OBJ;
    r->o = alcobj(ctx);
    int n = a0->o->idxsz;
    for (int i = 0; i < n; i++) {
        vmvar *v = alcvar_initial(ctx);
        COPY_VAR_TO(ctx, v, a0->o->ary[i]);
        array_push(ctx, r->o, v);
    }
    return 0;
}

static int XmlDom_firstChild(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    if (0 < a0->o->idxsz) {
        COPY_VAR_TO(ctx, r, a0->o->ary[0]);
    } else {
        SET_UNDEF(r);
    }
    return 0;
}

static int XmlDom_nextSibling(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    int i = -1;
    vmvar *index = hashmap_search(a0->o, "_$index");
    if (index->t == VAR_INT64) {
        i = index->i + 1;
    }
    vmvar *v = hashmap_search(a0->o, "_$parent");
    if (v && v->t == VAR_OBJ && 0 < i && i < v->o->idxsz) {
        COPY_VAR_TO(ctx, r, v->o->ary[i]);
    } else {
        SET_UNDEF(r);
    }
    return 0;
}

static int XmlDom_prevSibling(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_OBJ);
    int i = -1;
    vmvar *index = hashmap_search(a0->o, "_$index");
    if (index->t == VAR_INT64) {
        i = index->i - 1;
    }
    vmvar *v = hashmap_search(a0->o, "_$parent");
    if (v && v->t == VAR_OBJ && 0 <= i && i < v->o->idxsz) {
        COPY_VAR_TO(ctx, r, v->o->ary[i]);
    } else {
        SET_UNDEF(r);
    }
    return 0;
}

static void setup_xmlnode_method(vmctx *ctx, vmfrm *lex, vmvar *r, vmvar *parent, int index, int type)
{
    vmobj *o = r->o;
    KL_SET_PROPERTY(o, _$parent, parent);
    KL_SET_PROPERTY_I(o, _$index, index);
    KL_SET_PROPERTY_I(o, type, type);
    KL_SET_METHOD(o, tagName, XmlDom_tagName, lex, 1);
    KL_SET_METHOD(o, text, XmlDom_text, lex, 1);
    KL_SET_METHOD(o, attributes, XmlDom_attributes, lex, 1);
    KL_SET_METHOD(o, hasChildNodes, XmlDom_hasChildNodes, lex, 1);
    KL_SET_METHOD(o, parentNode, XmlDom_parentNode, lex, 1);
    KL_SET_METHOD(o, childNodes, XmlDom_childNodes, lex, 1);
    KL_SET_METHOD(o, firstChild, XmlDom_firstChild, lex, 1);
    KL_SET_METHOD(o, nextSibling, XmlDom_nextSibling, lex, 1);
    KL_SET_METHOD(o, prevSibling, XmlDom_prevSibling, lex, 1);
    o->is_sysobj = 1;
}

static const char *parse_doc(vmctx *ctx, vmfrm *lex, vmvar *r, const char *p, int *err, int *line, int *pos, int depth)
{
    while (*p != 0) {
        const char *s = p;
        while (*p != '<') {
            if (*p == 0) break;
            move_next(p);
        }
        if (s != p) {
            vmvar *vs = make_str_obj(ctx, s, p - s);
            vmvar *vo = alcvar_obj(ctx, alcobj(ctx));
            KL_SET_PROPERTY(vo->o, _$text, vs);
            setup_xmlnode_method(ctx, lex, vo, r, r->o->idxsz, XMLDOM_TEXT_NODE);
            array_push(ctx, r->o, vo);
        }
        if (*p == 0) break;
        if (*p == '<') {
            move_next(p);
            if (*p == '/') {
                return p;
            }
            vmvar *n = alcvar_obj(ctx, alcobj(ctx));
            if (*p == '?') {
                move_next(p);
                int xmldecl = 0;
                p = patse_pi(ctx, r, p, err, line, pos, &xmldecl);
                if (xmldecl && (depth > 0 || r->o->idxsz > 0)) {
                    errset_wuth_ret(err, XMLDOM_EDECL);
                }
                setup_xmlnode_method(ctx, lex, n, r, r->o->idxsz, xmldecl ? XMLDOM_XMLDECL_NODE : XMLDOM_PROCESSING_INSTRUCTION_NODE);
            } else {
                p = patse_node(ctx, lex, n, p, err, line, pos, depth);
                setup_xmlnode_method(ctx, lex, n, r, r->o->idxsz, XMLDOM_ELEMENT_NODE);
            }
            if (*err < 0) {
                return p;
            }
            array_push(ctx, r->o, n);
        }
        if (*p == 0) break;
        if (*p != '>') {
            errset_wuth_ret(err, XMLDOM_ESYNTAX);
        }
        move_next(p);
    }
    return p;
}

static int XmlDom_parseString(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    DEF_ARG(a0, 0, VAR_STR);
    vmstr *s = a0->s;
    const char *str = s->s;
    r->t = VAR_OBJ;
    r->o = alcobj(ctx);
    r->o->is_sysobj = 1;
    if (s->s) {
        int err = 0;
        int line = 1;
        int pos = 0;
        parse_doc(ctx, lex, r, str, &err, &line, &pos, 0);
        KL_SET_METHOD(r->o, documentElement, XmlDom_documentElement, lex, 1);
        if (err < 0) {
            char buf[256] = {0};
            sprintf(buf, "Xml parse error at %d:%d", line, pos);
            return throw_system_exception(__LINE__, ctx, EXCEPT_XML_ERROR, buf);
        }
    }
    return 0;
}

int XmlDom(vmctx *ctx, vmfrm *lex, vmvar *r, int ac)
{
    vmobj *o = alcobj(ctx);
    ctx->s = o;
    KL_SET_METHOD(o, parseString, XmlDom_parseString, lex, 1);
    KL_SET_PROPERTY_I(o, XMLDOM_XMLDECL_NODE,                       XMLDOM_XMLDECL_NODE);
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
