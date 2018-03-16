#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#ifdef WIN32
#define __STDC_FORMAT_MACROS // Enable integer types
#endif
#include <inttypes.h>

#include "lcmgen.h"

#define INDENT(n) (4*(n))
#define emit_start(n, ...) \
    do { fprintf(f, "%*s", INDENT(n), ""); fprintf(f, __VA_ARGS__); } while (0)
#define emit_continue(...) \
    do { fprintf(f, __VA_ARGS__); } while (0)
#define emit_end(...) \
    do { fprintf(f, __VA_ARGS__); fprintf(f, "\n"); } while (0)
#define emit(n, ...) \
    do { fprintf(f, "%*s", INDENT(n), ""); fprintf(f, __VA_ARGS__); \
        fprintf(f, "\n"); } while (0)
#define emit_nl() \
    do { fprintf(f, "\n"); } while (0)

static char * dots_to_underscores(const char *s)
{
    char *p = strdup(s);

    for (char *t=p; *t!=0; t++)
        if (*t == '.')
            *t = '_';

    return p;
}

static const char * first_to_upper(const char *str)
{
    char *s = strdup(str);
    s[0] = toupper(s[0]);
    return s;
}

static void emit_comment(FILE *f, int indent, const char *comment)
{
    if (!comment)
        return;

    gchar **lines = g_strsplit(comment, "\n", 0);
    for (int line = 0; lines[line]; line++) {
        emit(indent, "// %s", lines[line]);
    }

    g_strfreev(lines);
}

static void emit_auto_generated_warning(FILE *f)
{
    fprintf(f,
            "// THIS IS AN AUTOMATICALLY GENERATED FILE.  DO NOT MODIFY\n"
            "// BY HAND!!\n"
            "//\n"
            "// Generated by lcm-gen\n\n");
}

static const char *map_builtintype_name(const char *t)
{
    if (!strcmp(t,"boolean")) return "bool";
    if (!strcmp(t,"byte")) return "byte";
    if (!strcmp(t,"int8_t")) return "int8";
    if (!strcmp(t,"int16_t")) return "int16";
    if (!strcmp(t,"int32_t")) return "int32";
    if (!strcmp(t,"int64_t")) return "int64";
    if (!strcmp(t,"float")) return "float32";
    if (!strcmp(t,"double")) return "float64";
    if (!strcmp(t,"string")) return "string";
    return NULL;
}

static const char *map_type_name(const char *t)
{
    const char *builtin = map_builtintype_name(t);
    if (builtin != NULL) {
        return strdup(builtin);
    }

    // In case none of the above
    char *tn = dots_to_underscores(t);
    size_t len = strlen(tn) * 2 + 2;
    char *tm = (char *)malloc(len);
    char *to = (char *)first_to_upper(tn);
    snprintf(tm, len, "%s.%s", tn, to);
    free(tn);
    free(to);

    return tm;
}

static int primitive_type_size (const char *tn)
{
    if (!strcmp("byte", tn)) return 1;
    if (!strcmp("boolean", tn)) return 1;
    if (!strcmp("int8_t", tn)) return 1;
    if (!strcmp("int16_t", tn)) return 2;
    if (!strcmp("int32_t", tn)) return 4;
    if (!strcmp("int64_t", tn)) return 8;
    if (!strcmp("float", tn)) return 4;
    if (!strcmp("double", tn)) return 8;
    if (!strcmp("string", tn)) return 100; // TODO FIXME
    assert (0);
    return 0;
}

static void emit_encode_function(FILE *f, const char *type, const char *src,
    int indent)
{
    if (!strcmp(type, "boolean")) {
        emit(indent, "if p.%s {", src);
        emit(indent + 1, "data[offset] = 1");
        emit(indent, "} else {");
        emit(indent + 1, "data[offset] = 0");
        emit(indent, "}");
        emit(indent, "offset += 1");
    } else if (!strcmp(type, "byte")) {
        emit(indent, "data[offset] = p.%s", src);
        emit(indent, "offset += 1");
    } else if (!strcmp(type, "int8_t")) {
        emit(indent, "data[offset] = byte(p.%s)", src);
        emit(indent, "offset += 1");
    } else if (!strcmp(type, "int16_t")) {
        emit(indent, "binary.BigEndian.PutUint16(data[offset:],");
        emit(indent + 1, "uint16(p.%s))", src);
        emit(indent, "offset += 2");
    } else if (!strcmp(type, "int32_t")) {
        emit(indent, "binary.BigEndian.PutUint32(data[offset:],");
        emit(indent + 1, "uint32(p.%s))", src);
        emit(indent, "offset += 4");
    } else if (!strcmp(type, "int64_t")) {
        emit(indent, "binary.BigEndian.PutUint64(data[offset:],");
        emit(indent + 1, "uint64(p.%s))", src);
        emit(indent, "offset += 8");
    } else if (!strcmp(type, "float")) {
        emit(indent, "binary.BigEndian.PutUint32(data[offset:],");
        emit(indent + 1, "math.Float32bits(p.%s))", src);
        emit(indent, "offset += 4");
    } else if (!strcmp(type, "double")) {
        emit(indent, "binary.BigEndian.PutUint64(data[offset:],");
        emit(indent + 1, "math.Float64bits(p.%s))", src);
        emit(indent, "offset += 8");
    } else if (!strcmp(type, "string")) {
        emit(indent, "{");
        emit(indent + 1, "bstr := []byte(p.%s)", src);
        emit(indent + 1, "binary.BigEndian.PutUint32(data[offset:],");
        emit(indent + 2, "uint32(len(bstr))+1)");
        emit(indent + 1, "offset += 4");
        emit(indent + 1, "offset += copy(data[offset:], bstr)");
        emit(indent + 1, "data[offset] = 0");
        emit(indent + 1, "offset += 1");
        emit(indent, "}");
    }
}

static void emit_decode_function(FILE *f, const char *type, const char *dst,
    int indent)
{
    if (!strcmp(type, "boolean")) {
        emit(indent, "if data[offset] != 0 {");
        emit(indent + 1, "p.%s = true", dst);
        emit(indent, "} else {");
        emit(indent + 1, "p.%s = false", dst);
        emit(indent, "}");
        emit(indent, "offset += 1");
    } else if (!strcmp(type, "byte")) {
        emit(indent, "p.%s = data[offset]", dst);
        emit(indent, "offset += 1");
    } else if (!strcmp(type, "int8_t")) {
        emit(indent, "p.%s = int8(data[offset])", dst);
        emit(indent, "offset += 1");
    } else if (!strcmp(type, "int16_t")) {
        emit(indent, "p.%s = int16(binary.BigEndian.Uint16(data[offset:]))", dst);
        emit(indent, "offset += 2");
    } else if (!strcmp(type, "int32_t")) {
        emit(indent, "p.%s = int32(binary.BigEndian.Uint32(data[offset:]))", dst);
        emit(indent, "offset += 4");
    } else if (!strcmp(type, "int64_t")) {
        emit(indent, "p.%s = int64(binary.BigEndian.Uint64(data[offset:]))", dst);
        emit(indent, "offset += 8");
    } else if (!strcmp(type, "float")) {
        emit(indent, "p.%s = math.Float32frombits(binary.BigEndian.Uint32(data[offset:]))", dst);
        emit(indent, "offset += 4");
    } else if (!strcmp(type, "double")) {
        emit(indent, "p.%s = math.Float64frombits(binary.BigEndian.Uint64(data[offset:]))", dst);
        emit(indent, "offset += 8");
    } else if (!strcmp(type, "string")) {
        emit(indent, "{");
        emit(indent + 1, "length := int(binary.BigEndian.Uint32(data[offset:]))");
        emit(indent + 1, "offset += 4");
        emit(indent + 1, "if length < 1 {");
        emit(indent + 2, "return fmt.Errorf(\"Decoded string length is negative\")");
        emit(indent + 1, "}");
        emit(indent + 1, "p.%s = string(data[offset:offset + length-1])", dst);
        emit(indent + 1, "offset += length");
        emit(indent, "}");
    }
}

static void emit_go_header(FILE *f, const char *gopacket)
{
    emit_auto_generated_warning(f);
    emit(0, "package %s", gopacket);
    emit_nl();
}

static void emit_go_imports(FILE *f, lcm_struct_t *ls) {
    emit(0, "import (");
    emit(1, "\"math\"");
    emit(1, "\"math/bits\"");
    emit(1, "\"encoding/binary\"");
    emit(1, "\"fmt\"");
    for (unsigned int i = 0; i < ls->members->len; i++) {
        lcm_member_t *lm = (lcm_member_t *)g_ptr_array_index(ls->members, i);
        if (!lcm_is_primitive_type(lm->type->lctypename)) {
            const char *packet = dots_to_underscores(lm->type->lctypename);
            emit(1, "\"%s\"", packet);
            free((char *)packet);
        }
    }
    emit(0, ")");
    emit_nl();

    // TODO To silence (possible) not used import warning by go compiler
    emit(0, "const _ = math.Pi");
    emit_nl();
}

static void emit_go_struct_definition(FILE *f, lcm_struct_t *ls, const char *gotype) {
    emit(0, "type %s struct {", gotype);

    for (unsigned int i = 0; i < ls->members->len; i++) {
        lcm_member_t *lm = (lcm_member_t *)g_ptr_array_index(ls->members, i);

        emit_comment(f, 1, lm->comment);

        int ndim = lm->dimensions->len;

        if (ndim == 0) {
            const char *_membername = dots_to_underscores(lm->membername);
            const char *membername = first_to_upper(_membername);
            const char *membertype = map_type_name(lm->type->lctypename);
            emit(1, "%-20s %s", membername, membertype);
            free((char *)_membername);
            free((char *)membername);
            free((char *)membertype);
        } else {
            const char *_membername = dots_to_underscores(lm->membername);
            const char *membername = first_to_upper(_membername);
            emit_start(1, "%-20s ", membername);
            free((char *)_membername);
            free((char *)membername);

            for (unsigned int d = 0; d < ndim; d++) {
                lcm_dimension_t *dim = (lcm_dimension_t *)g_ptr_array_index(lm->dimensions, d);
                if (dim->mode == LCM_CONST) {
                    emit_continue("[%s]", dim->size);
                } else {
                    emit_continue("[]");
                }
            }

            const char *membertype = map_type_name(lm->type->lctypename);
            emit_end("%s", membertype);
            free((char *)membertype);
        }
    }

    emit(0, "}");
    emit_nl();
}

static void emit_go_deep_copy(FILE *f, lcm_struct_t *ls, const char *gotype) {
    emit(0, "// Copy creates a deep copy");
    emit(0, "func (p *%s) Copy() (dst %s) {", gotype, gotype);

    for (unsigned int m = 0; m < ls->members->len; m++) {
        lcm_member_t *lm = (lcm_member_t *)g_ptr_array_index(ls->members, m);

        emit_comment(f, 1, lm->comment);

        const char *_membername = dots_to_underscores(lm->membername);
        const char *membername = first_to_upper(_membername);

        if (lcm_is_primitive_type(lm->type->lctypename)) {

            if (!lm->dimensions->len) {
                emit(1, "dst.%s = p.%s", membername, membername);
            } else {
                unsigned int n;
                GString *arraystr = g_string_new(membername);
                for (n = 0; n<lm->dimensions->len; n++) {
                    lcm_dimension_t *dim =
                        (lcm_dimension_t*)g_ptr_array_index(lm->dimensions, n);
                    g_string_append_printf(arraystr, "[i%d]", n);
                    if (dim->mode == LCM_VAR) {
                        const char *v = first_to_upper(dim->size);
                        emit(1+n, "for i%d := %s(0); i%d < p.%s; i%d++ {", n,
                            map_builtintype_name(lcm_find_member(ls, dim->size)->type->lctypename),
                            n, v, n);
                        free((char *)v);
                    } else {
                        emit(1+n, "for i%d := 0; i%d < %s; i%d++ {", n, n,
                            dim->size, n);
                    }
                }
                emit(n + 1, "dst.%s = p.%s", arraystr->str, arraystr->str);
                g_string_free(arraystr, TRUE);

                for (n = lm->dimensions->len; n > 0; n--)
                    emit(n, "}");
            }
        } else {
            if (!lm->dimensions->len) {
                emit(1, "dst.%s = p.%s.Copy()", membername, membername);
            } else {
                unsigned int n;
                GString *arraystr = g_string_new(NULL);
                for (n = 0; n<lm->dimensions->len; n++) {
                    lcm_dimension_t *dim =
                        (lcm_dimension_t*)g_ptr_array_index(lm->dimensions, n);
                    g_string_append_printf(arraystr, "[i%d]", n);
                    if (dim->mode == LCM_VAR) {
                        const char *v = first_to_upper(dim->size);
                        emit(1+n, "for i%d := %s(0); i%d < p.%s; i%d++ {", n,
                            map_builtintype_name(lcm_find_member(ls, dim->size)->type->lctypename),
                            n, v, n);
                        free((char *)v);
                    } else {
                        emit(1+n, "for i%d := 0; i%d < %s; i%d++ {", n, n,
                            dim->size, n);
                    }
                }
                emit(n + 1, "dst.%s%s = p.%s%s.Copy()", membername,
                     arraystr->str, membername, arraystr->str);
                g_string_free(arraystr, TRUE);

                for (n = lm->dimensions->len; n > 0; n--)
                    emit(n, "}");
            }
        }
        emit_nl();

        free((char *)_membername);
        free((char *)membername);
    }
    emit(1, "return");
    emit(0, "}");
    emit_nl();
}

static void emit_go_encode(FILE *f, lcm_struct_t *ls, const char *gotype) {
    emit(0, "// Encode encodes a message (fingerprint & data) into binary form");
    emit(0, "//");
    emit(0, "// returns Encoded data or error");
    emit(0, "func (p *%s) Encode() (data []byte, err error) {", gotype);
    emit(1, "size := p.Size()");
    emit(1, "data = make([]byte, 8 + size)");
    emit(1, "binary.BigEndian.PutUint64(data, p.Fingerprint())");
    emit_nl();
    emit(1, "var d []byte");
    emit(1, "if d, err = p.MarshalBinary(); err != nil {");
    emit(2, "return");
    emit(1, "}");
    emit_nl();
    emit(1, "if copied := copy(data[8:], d); copied != size {");
    emit(2, "return data,");
    emit(3, "fmt.Errorf(\"Encoding error, buffer not filled (%%v != %%v)\", copied, size)");
    emit(1, "}");
    emit(1, "return");
    emit(0, "}");
    emit_nl();
}

static void emit_go_marshal_binary(FILE *f, lcm_struct_t *ls, const char *gotype) {
    emit(0, "// MarshalBinary implements the BinaryMarshaller interface");
    emit(0, "func (p *%s) MarshalBinary() (data []byte, err error) {", gotype);
    if (ls->members->len) {
        emit(1, "data = make([]byte, p.Size())");
        emit(1, "offset := 0");
        emit_nl();
    }

    for (unsigned int i = 0; i < ls->members->len; i++) {
        lcm_member_t *lm = (lcm_member_t *)g_ptr_array_index(ls->members, i);

        emit_comment(f, 1, lm->comment);

        const char *_membername = dots_to_underscores(lm->membername);
        const char *membername = first_to_upper(_membername);

        if (lcm_is_primitive_type(lm->type->lctypename)) {
            if (!lm->dimensions->len) {
                // TODO split this into separate function
                if (lcm_is_array_dimension_type(lm->type->lctypename)) {
                    // Find the arrays in which it is used
                    // Verify size is eq to their length
                    for (unsigned int j = i; j < ls->members->len; j++) {
                        lcm_member_t *lm_ = (lcm_member_t *) g_ptr_array_index(ls->members, j);
                        if (lm_->dimensions->len) {
                            const char *batman = first_to_upper(lm_->membername);
                            GString *joker = g_string_new(batman);
                            free((char *)batman);
                            GString *twoface = g_string_new("");
                            unsigned int batgirl = 0;
                            for (unsigned int k = 0; k < lm_->dimensions->len; k++) {
                                lcm_dimension_t *dim = (lcm_dimension_t *) g_ptr_array_index (lm_->dimensions, k);
                                if (dim->mode == LCM_VAR) {
                                    if (strcmp(lm->membername, dim->size) == 0) {
                                        emit_start(0, "%s", twoface->str);
                                        batgirl = k;
                                        g_string_truncate(twoface, 0);
                                        emit(k+1, "if p.%s != 0 &&", membername);
                                        emit(k+2, "int(p.%s) != len(p.%s) {", membername, joker->str);
                                        emit(k+2, "return data, fmt.Errorf(\"Defined dynamic array size not matching actual\" +");
                                        emit(k+3, "\" array size (got %%v expected 0 or %%d for %s)\",", joker->str);
                                        emit(k+3, " p.%s, len(p.%s))", membername, joker->str);
                                        emit(k+1, "}");
                                    }
                                    const char *size = first_to_upper(dim->size);
                                    g_string_append_printf(twoface, "%*sfor i%d := %s(0); i%d < p.%s; i%d++ {\n",
                                        INDENT(k+1), "", k,
                                        map_builtintype_name(lcm_find_member(ls, dim->size)->type->lctypename),
                                        k, size, k);
                                    free((char *)size);
                                } else {
                                    g_string_append_printf(twoface, "%*sfor i%d := 0; i%d < %s; i%d++ {\n",
                                        INDENT(k+1), "", k, k, dim->size, k);
                                }
                                g_string_append_printf(joker, "[i%d]", k);
                            }
                            for (unsigned int k = batgirl; k > 0; k--)
                                emit(k, "}");
                            g_string_free(joker, TRUE);
                            g_string_free(twoface, TRUE);
                        }
                    }
                }
                emit_encode_function(f, lm->type->lctypename,
                    membername, 1);
            } else {
                unsigned int n;
                GString *arraystr = g_string_new(membername);
                for (n = 0; n<lm->dimensions->len; n++) {
                    lcm_dimension_t *dim =
                        (lcm_dimension_t*)g_ptr_array_index(lm->dimensions, n);
                    g_string_append_printf(arraystr, "[i%d]", n);
                    if (dim->mode == LCM_VAR) {
                        const char *v = first_to_upper(dim->size);
                        emit(1+n, "for i%d := %s(0); i%d < p.%s; i%d++ {", n,
                            map_builtintype_name(lcm_find_member(ls, dim->size)->type->lctypename),
                            n, v, n);
                        free((char *)v);
                    } else {
                        emit(1+n, "for i%d := 0; i%d < %s; i%d++ {", n, n,
                            dim->size, n);
                    }
                }
                emit_encode_function(f, lm->type->lctypename,
                    arraystr->str, 1+n);
                g_string_free(arraystr, TRUE);

                for (n = lm->dimensions->len; n > 0; n--)
                    emit(n, "}");
            }
        } else {
            if (!lm->dimensions->len) {
                emit(1, "d, e := p.%s.MarshalBinary(); if e != nil {", membername);
                emit(2, "return data, e");
                emit(1, "}");
                emit(1, "offset += copy(data[offset:], d)");
            } else {
                unsigned int n;
                GString *arraystr = g_string_new(NULL);
                for (n = 0; n<lm->dimensions->len; n++) {
                    lcm_dimension_t *dim =
                        (lcm_dimension_t*)g_ptr_array_index(lm->dimensions, n);
                    g_string_append_printf(arraystr, "[i%d]", n);
                    if (dim->mode == LCM_VAR) {
                        const char *v = first_to_upper(dim->size);
                        emit(1+n, "for i%d := %s(0); i%d < p.%s; i%d++ {", n,
                            map_builtintype_name(lcm_find_member(ls, dim->size)->type->lctypename),
                            n, v, n);
                        free((char *)v);
                    } else {
                        emit(1+n, "for i%d := 0; i%d < %s; i%d++ {", n, n,
                            dim->size, n);
                    }
                }
                emit(2, "d, e := p.%s%s.MarshalBinary(); if e != nil {",
                     membername, arraystr->str);
                emit(3, "return data, e");
                emit(2, "}");
                emit(2, "offset += copy(data[offset:], d)");
                g_string_free(arraystr, TRUE);

                for (n = lm->dimensions->len; n > 0; n--)
                    emit(n, "}");
            }
        }
        emit_nl();
        free((char *)_membername);
        free((char *)membername);
    }
    emit(1, "return");
    emit(0, "}");
    emit_nl();
}

static void emit_go_decode(FILE *f, lcm_struct_t *ls, const char *gotype) {
    emit(0, "// Decode decodes a message (fingerprint & data) from binary form");
    emit(0, "// and verifies that the fingerprint match the expected");
    emit(0, "//");
    emit(0, "// param data The buffer containing the encoded message");
    emit(0, "// returns Error");
    emit(0, "func (p *%s) Decode(data []byte) (err error) {", gotype);
    emit(1, "length := len(data)");
    emit(1, "if length < 8 {");
    emit(2, "return fmt.Errorf(\"Missing fingerprint in buffer\")");
    emit(1, "}");
    emit_nl();
    emit(1, "if fp := binary.BigEndian.Uint64(data[:8]);");
    emit(2, "fp != p.Fingerprint() {");
    emit(2, "return fmt.Errorf(\"Fingerprints does not match (got %%x expected %%x)\",");
    emit(3, "fp, p.Fingerprint())");
    emit(1, "}");
    emit_nl();
    emit(1, "length -= 8");
    emit(1, "size := p.Size()");
    emit(1, "if length != size {");
    emit(2, "return fmt.Errorf(\"Missing data in buffer (size missmatch, got %%v expected %%v)\",");
    emit(3, "length, size)");
    emit(1, "}");
    emit_nl();
    emit(1, "return p.UnmarshalBinary(data[8:])");
    emit(0, "}");
    emit_nl();
}

static void emit_go_unmarshal_binary(FILE *f, lcm_struct_t *ls, const char *gotype) {
    int readVar = 0;
    emit(0, "// UnmarshalBinary implements the BinaryUnmarshaler interface");
    emit(0, "func (p *%s) UnmarshalBinary(data []byte) (err error) {", gotype);
    if (ls->members->len) {
        emit(1, "offset := 0");
        emit_nl();
    }

    for (unsigned int m = 0; m < ls->members->len; m++) {
        lcm_member_t *lm = (lcm_member_t *) g_ptr_array_index(ls->members, m);

        emit_comment(f, 1, lm->comment);

        const char *_membername = dots_to_underscores(lm->membername);
        const char *membername = first_to_upper(_membername);

        if (lcm_is_primitive_type(lm->type->lctypename)) {
            if (!lm->dimensions->len) {
                emit_decode_function(f, lm->type->lctypename, membername, 1);
            } else {
                unsigned int n;
                GString *arraystr = g_string_new(membername);
                for (n = 0; n<lm->dimensions->len; n++) {
                    lcm_dimension_t *dim =
                        (lcm_dimension_t*)g_ptr_array_index(lm->dimensions, n);
                    g_string_append_printf(arraystr, "[i%d]", n);
                    if (dim->mode == LCM_VAR) {
                        const char *v = first_to_upper(dim->size);
                        emit(1+n, "for i%d := %s(0); i%d < p.%s; i%d++ {", n,
                            map_builtintype_name(lcm_find_member(ls, dim->size)->type->lctypename),
                            n, v, n);
                        free((char *)v);
                    } else {
                        emit(1+n, "for i%d := 0; i%d < %s; i%d++ {", n, n,
                            dim->size, n);
                    }
                }
                emit_decode_function(f, lm->type->lctypename, arraystr->str, 1+n);
                g_string_free(arraystr, TRUE);

                for (n = lm->dimensions->len; n > 0; n--)
                    emit(n, "}");
            }
        } else {
            if (!lm->dimensions->len) {
                emit(1, "err = p.%s.UnmarshalBinary(data[offset:]); if err != nil {", membername);
                emit(2, "return");
                emit(1, "}");
                emit(1, "offset += p.%s.Size()", membername);
            } else {
                unsigned int n;
                GString *arraystr = g_string_new(NULL);
                for (n = 0; n<lm->dimensions->len; n++) {
                    lcm_dimension_t *dim =
                        (lcm_dimension_t*)g_ptr_array_index(lm->dimensions, n);
                    g_string_append_printf(arraystr, "[i%d]", n);
                    if (dim->mode == LCM_VAR) {
                        const char *v = first_to_upper(dim->size);
                        emit(1+n, "for i%d := %s(0); i%d < p.%s; i%d++ {", n,
                            map_builtintype_name(lcm_find_member(ls, dim->size)->type->lctypename),
                            n, v, n);
                        free((char *)v);
                    } else {
                        emit(1+n, "for i%d := 0; i%d < %s; i%d++ {", n, n,
                            dim->size, n);
                    }
                }
                emit(n + 1, "err = p.%s%s.UnmarshalBinary(data[offset:]); if err != nil {", membername,
                    arraystr->str);
                emit(n + 2, "return");
                emit(n + 1, "}");
                emit(n + 1, "offset += p.%s%s.Size()", membername, arraystr->str);
                g_string_free(arraystr, TRUE);

                for (n = lm->dimensions->len; n > 0; n--)
                    emit(n, "}");
            }
        }
        emit_nl();
        free((char *)_membername);
        free((char *)membername);
    }
    emit(1, "return");
    emit(0, "}");
    emit_nl();
}

static void emit_go_fingerprint(FILE *f, lcm_struct_t *ls, const char *gotype) {
    emit(0, "// Fingerprint generates the LCM fingerprint value for this message");
    emit(0, "func (p *%s) Fingerprint(path ...uint64) uint64 {", gotype);
    emit(1, "var fingerprint uint64 = 0x%016"PRIx64, ls->hash);
    emit(1, "for _, v := range path {");
    emit(2, "if v == fingerprint {");
    emit(3, "return 0");
    emit(2, "}");
    emit(1, "}");
    emit_nl();

    emit(1, "path = append(path, fingerprint)");
    emit_start(1, "return bits.RotateLeft64(fingerprint");
    for (unsigned int m = 0; m < ls->members->len; m++) {
        lcm_member_t *lm = (lcm_member_t *)g_ptr_array_index(ls->members, m);

        if (!lcm_is_primitive_type(lm->type->lctypename)) {
            emit_end(" +");
            const char *_membername = dots_to_underscores(lm->membername);
            const char *membername = first_to_upper(_membername);
            emit_start(2, "p.%s.Fingerprint(path...)", membername);
            free((char *)_membername);
            free((char *)membername);
        }
    }
    emit_end(", 1)");
    emit(0, "}");
    emit_nl();
}

static void emit_go_size(FILE *f, lcm_struct_t *ls, const char *gotype) {
    emit(0, "// Size returns the size of this message in binary form");
    emit(0, "func (p *%s) Size() int {", gotype);
    if (!ls->members->len) {
        emit(1, "return 0");
    } else {
        emit_start(1, "return ");
        for (unsigned int m = 0; m < ls->members->len; m++) {
            lcm_member_t *lm = (lcm_member_t *)g_ptr_array_index(ls->members, m);
            if (lcm_is_primitive_type(lm->type->lctypename)) {
                emit_continue("%d", primitive_type_size(lm->type->lctypename));
            } else {
                const char *_membername = dots_to_underscores(lm->membername);
                const char *membername = first_to_upper(_membername);
                emit_continue("p.%s.Size()", membername);
                free((char *)_membername);
                free((char *)membername);
            }

            if (m < ls->members->len - 1) {
                emit_end(" +");
                emit_start(2, "");
            } else {
                emit_end("");
            }
        }
    }
    emit(0, "}");
}

int emit_go_enum(lcmgen_t *lcm, lcm_enum_t *le)
{
    char *tn = le->enumname->lctypename;
    char *tn_ = dots_to_underscores(tn);
    char *path = g_strdup_printf("%s/%s_enums.go",
          getopt_get_string(lcm->gopt, "go-path"),
		  tn_);

    if (!lcm_needs_generation(lcm, le->lcmfile, path))
        return 0;

    FILE *f = fopen(path, "w");
    if (f==NULL)
        return -1;

    emit_go_header(f, tn_);
    emit(0, "// TODO");

    free(tn_);

    return 0;
}

int emit_go_struct(lcmgen_t *lcm, lcm_struct_t *ls)
{
    const char *tn = ls->structname->lctypename;
    const char *gopacket = dots_to_underscores(tn);
    const char *gotype = first_to_upper(gopacket);
    const char *path = g_strdup_printf("%s/%s_structs.go",
                                       getopt_get_string(lcm->gopt, "go-path"),
		                               gopacket);

    if (!lcm_needs_generation(lcm, ls->lcmfile, path))
        return 0;

    FILE *f = fopen(path, "w");
    if (f==NULL)
        return -1;

    // Header
    emit_go_header(f, gopacket);

    // Imports
    emit_go_imports(f, ls);

    // Struct definition
    emit_go_struct_definition(f, ls, gotype);

    // Functions
    // Deep copy
    emit_go_deep_copy(f, ls, gotype);

    // Encode
    emit_go_encode(f, ls, gotype);

    // MarshalBinary
    emit_go_marshal_binary(f, ls, gotype);

    // Decode
    emit_go_decode(f, ls, gotype);

    // UnmarshalBinary
    emit_go_unmarshal_binary(f, ls, gotype);

    // Fingerprint
    emit_go_fingerprint(f, ls, gotype);

    // Size
    emit_go_size(f, ls, gotype);

    free((char *)gopacket);
    free((char *)gotype);
    free((char *)path);

    fclose(f);

    return 0;
}

void setup_go_options(getopt_t *gopt)
{
    getopt_add_string(gopt, 0, "go-path", ".", "Location for .go files");
}

int emit_go(lcmgen_t *lcmgen)
{
    ////////////////////////////////////////////////////////////
    // ENUMS
    for (unsigned int i = 0; i < lcmgen->enums->len; i++) {

        lcm_enum_t *le = (lcm_enum_t *) g_ptr_array_index(lcmgen->enums, i);
        if (emit_go_enum(lcmgen, le))
            return -1;
    }

    ////////////////////////////////////////////////////////////
    // STRUCTS
    for (unsigned int i = 0; i < lcmgen->structs->len; i++) {
        lcm_struct_t *ls = (lcm_struct_t *) g_ptr_array_index(lcmgen->structs, i);

        if (emit_go_struct(lcmgen, ls))
            return -1;
    }

    return 0;
}
