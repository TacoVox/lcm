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

/*
 * Simple function that takes a string like this.is.a.string and turns it into
 * this_is_a_string.
 *
 * CAUTION: memory has to be freed manually.
 */
static char * dots_to_underscores(const char *s)
{
    char *p = strdup(s);

    for (char *t=p; *t!=0; t++)
        if (*t == '.')
            *t = '_';

    return p;
}

/*
 * Simple function that takes a string like this_is_a_string and turns it into
 * This_is_a_string.
 *
 * CAUTION: memory has to be freed manually.
 */
static const char * first_to_upper(const char *str)
{
    char *s = strdup(str);
    s[0] = toupper(s[0]);
    return s;
}

/*
 * Returns the index of first member in which name is used as a variable
 * dimension or members->len if not found.
 * Starts at given index.
 */
unsigned int lcm_find_member_with_named_dimension(lcm_struct_t *ls,
    const char *name, unsigned int start) {

    for (unsigned int i = start; i < ls->members->len; i++) {
        lcm_member_t *lm = (lcm_member_t *) g_ptr_array_index(ls->members, i);
        for (unsigned int j = 0; j < lm->dimensions->len; j++) {
            lcm_dimension_t *dim = (lcm_dimension_t *) g_ptr_array_index(
                lm->dimensions, j);
            if (dim->mode == LCM_VAR) {
                if (strcmp(name, dim->size) == 0) {
                    return i;
                }
            }
        }
    }
    return ls->members->len;
}

/*
 * Returns the index of the first dimension named name or dimensions->len if
 * not found.
 * Starts at given index.
 */
unsigned int lcm_find_named_dimension(FILE *f, lcm_struct_t *ls, lcm_member_t *lm,
    const char *name, unsigned int start) {

    for (unsigned int i = start; i < lm->dimensions->len; i++) {
        lcm_dimension_t *dim = (lcm_dimension_t *) g_ptr_array_index(
            lm->dimensions, i);
        if (dim->mode == LCM_VAR) {
            if (strcmp(name, dim->size) == 0) {
                return i;
            }
        }
    }
    return lm->dimensions->len;
}

/*
 * Takes a typical LCM membername like rapid.test and turns it into something
 * usable in go.
 *
 * CAUTION: memory has to be freed manually.
 */
static const char * go_membername(FILE *f, lcm_struct_t *ls, const char *str,
    int method) {
    char *membername = dots_to_underscores(str);

    if (lcm_find_member_with_named_dimension(ls, str, 0) >= ls->members->len) {
        // If not a read-only attribute, uppercase it to export it.
        membername[0] = toupper(membername[0]);
    } else if (method) {
        // If read-only should be method invocation or not
        size_t len = strlen(membername);
        membername = realloc(membername, len + 3);
        membername[0] = toupper(membername[0]);
        membername[len++] = '(';
        membername[len++] = ')';
        membername[len++] = '\0';
    }

    return (const char *) membername;
}

/*
 * Emits a LCM comment.
 */
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

/*
 * Returns the Golang corresponding type of an LCM type.
 */
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
    assert(0);
    return NULL;
}

/*
 * More sofisticated than map_buildin_type(const char *). If it cannot find a
 * corresponding buildin type, this function assumes that we are dealing with
 * nested types. By doing so, it uses string manipulation to make the types
 * work with Golang.
 *
 * CAUTION: memory has to be freed manually.
 */
static const char *map_type_name(const char *t)
{
    if (lcm_is_primitive_type(t)) {
        return strdup(map_builtintype_name(t));
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

/*
 * Returns the size of a primitive type (string excluded) in bytes.
 */
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
    assert (0);
    return 0;
}

/*
 * Emits the decoding function for primitive types.
 */
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
    } else {
        assert(0);
    }
}

/*
 * Emits the encoding function for primitive types.
 */
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

/*
 * In order to fill slices, they have to be made first. This function emits
 * the necessary Go code when called. As more than one slice has to be created
 * when they are nested, this function takes start_dim as an argument.
 */
static void emit_go_slice_make(FILE *f, int indent, lcm_member_t *lm,
                               unsigned int start_dim, const char *name,
                               const char *size) {

    emit_start(indent, "p.%s = make(", name);

    for (unsigned int i = start_dim; i < lm->dimensions->len; i++) {
        lcm_dimension_t *dim =
            (lcm_dimension_t*)g_ptr_array_index(lm->dimensions, i);

        if (dim->mode == LCM_VAR) {
            emit_continue("[]");
        } else {
            emit_continue("[%s]", dim->size);
        }
    }

    const char *type = map_type_name(lm->type->lctypename);
    emit_end("%s, p.%s)", type, size);
    free((char *)type);
}

/*
 * Function to emit (nested) for loop(s), as this has to happen quite often in
 * case of multi-dimensional arrays.
 */
static unsigned int emit_go_array_loops(FILE *f, lcm_struct_t *ls,
                                        lcm_member_t *lm, GString *arraystr,
                                        int slice_emit, unsigned int end) {
    unsigned int n;
    GString *slicestr = g_string_new(NULL);

    for (n = 0; n < end; n++) {
        lcm_dimension_t *dim =
            (lcm_dimension_t*)g_ptr_array_index(lm->dimensions, n);

        if (slice_emit)
            g_string_assign(slicestr, arraystr->str);

        g_string_append_printf(arraystr, "[i%d]", n);

        if (dim->mode == LCM_VAR) {
            const char *size = go_membername(f, ls, dim->size, FALSE);
            const char *type = map_builtintype_name(
                lcm_find_member(ls, dim->size)->type->lctypename);

            if (slice_emit)
                emit_go_slice_make(f, n + 1, lm, n, slicestr->str, size);

            emit(1 + n, "for i%d := %s(0); i%d < p.%s; i%d++ {",
                n, type, n, size, n);

            free((char *)size);
        } else {
            emit(1 + n, "for i%d := 0; i%d < %s; i%d++ {", n, n, dim->size, n);
        }
    }

    g_string_free(slicestr, TRUE);

    return n;
}

/*
 * Ends the loops created with emit_go_array_loops(...).
 */
static void emit_go_array_loops_end(FILE *f, unsigned int n) {
    for (; n > 0; n--)
        emit(n, "}");
}

/*
 * Emits the header of the .go file.
 */
static void emit_go_header(FILE *f, const char *gopacket)
{
    emit_auto_generated_warning(f);
    emit(0, "package %s", gopacket);
    emit_nl();
}

/*
 * Emits the import of required packages.
 */
static void emit_go_imports(FILE *f, lcm_struct_t *ls) {
    emit(0, "import (");
    emit(1, "\"math\"");
    emit(1, "\"math/bits\"");
    emit(1, "\"encoding/binary\"");
    emit(1, "\"fmt\"");
    for (unsigned int i = 0; i < ls->members->len; i++) {
        lcm_member_t *lm = (lcm_member_t *)g_ptr_array_index(ls->members, i);

        if (lcm_is_primitive_type(lm->type->lctypename))
                continue;

        int imported = 0;
        for (unsigned int j = i - 1; j > 0; j--) {
            lcm_member_t *l = (lcm_member_t *)g_ptr_array_index(ls->members, j);

            if (strcmp(lm->type->lctypename, l->type->lctypename) == 0)
                imported = 1;
        }

        if (!imported) {
            const char *packet = dots_to_underscores(lm->type->lctypename);
            emit(1, "\"%s\"", packet);
            free((char *)packet);
        }
    }
    emit(0, ")");
    emit_nl();

    // Silence (possible) not used import warning by go compiler
    emit(0, "const _ = math.Pi");
    emit_nl();
}

/*
 * Emits the fingerprint of this particular struct as a constant.
 */
static void emit_go_fingerprint_const(FILE *f, lcm_struct_t *ls) {
    emit(0, "const fingerprint uint64 = 0x%016"PRIx64, ls->hash);
    emit_nl();
}

/*
 * Emits the struct's defintion.
 */
static void emit_go_struct_definition(FILE *f, lcm_struct_t *ls, const char *gotype) {
    unsigned int max_member_len = 0;

    for (unsigned int i = 0; i < ls->members->len; i++) {
        lcm_member_t *lm = (lcm_member_t *)g_ptr_array_index(ls->members, i);

        if (strlen(lm->membername) > max_member_len)
            max_member_len = strlen(lm->membername);
    }

    emit(0, "type %s struct {", gotype);

    for (unsigned int i = 0; i < ls->members->len; i++) {
        lcm_member_t *lm = (lcm_member_t *)g_ptr_array_index(ls->members, i);

        emit_comment(f, 1, lm->comment);

        const char *membername = go_membername(f, ls, lm->membername, FALSE);
        const char *membertype = map_type_name(lm->type->lctypename);

        GString *arraystr = g_string_new(NULL);

        for (unsigned int d = 0; d < lm->dimensions->len; d++) {
            lcm_dimension_t *dim = (lcm_dimension_t *)g_ptr_array_index(lm->dimensions, d);
            if (dim->mode == LCM_CONST) {
                g_string_append_printf(arraystr, "[%s]", dim->size);
            } else {
                g_string_append(arraystr, "[]");
            }
        }

        int spaces = max_member_len - strlen(membername) + 1;
        emit(1, "%s%*s%s%s", membername, spaces, "", arraystr->str,  membertype);

        free((char *)membername);
        free((char *)membertype);
        g_string_free(arraystr, TRUE);
    }

    emit(0, "}");
    emit_nl();
}

/*
 * This function emits code that will create an exact copy of the struct.
 */
static void emit_go_deep_copy(FILE *f, lcm_struct_t *ls, const char *gotype) {
    emit(0, "// Copy creates a deep copy");
    emit(0, "func (p *%s) Copy() (dst %s) {", gotype, gotype);

    for (unsigned int m = 0; m < ls->members->len; m++) {
        lcm_member_t *lm = (lcm_member_t *)g_ptr_array_index(ls->members, m);

        emit_comment(f, 1, lm->comment);

        const char *membername = go_membername(f, ls, lm->membername, FALSE);

        if (lcm_is_primitive_type(lm->type->lctypename)) {

            if (!lm->dimensions->len) {
                emit(1, "dst.%s = p.%s", membername, membername);
            } else {
                GString *arraystr = g_string_new(membername);
                unsigned int n = emit_go_array_loops(f, ls, lm, arraystr, FALSE, lm->dimensions->len);
                emit(n + 1, "dst.%s = p.%s", arraystr->str, arraystr->str);
                emit_go_array_loops_end(f, n);
                g_string_free(arraystr, TRUE);
            }
        } else {
            if (!lm->dimensions->len) {
                emit(1, "dst.%s = p.%s.Copy()", membername, membername);
            } else {
                GString *arraystr = g_string_new(NULL);
                unsigned int n = emit_go_array_loops(f, ls, lm, arraystr, FALSE, lm->dimensions->len);
                emit(n + 1, "dst.%s%s = p.%s%s.Copy()", membername,
                        arraystr->str, membername, arraystr->str);
                emit_go_array_loops_end(f, n);
                g_string_free(arraystr, TRUE);
            }
        }

        emit_nl();
        free((char *)membername);
    }

    emit(1, "return");
    emit(0, "}");
    emit_nl();
}

/*
 * Emits the main decode function.
 */
static void emit_go_encode(FILE *f, lcm_struct_t *ls, const char *gotype) {
    emit(0, "// Encode encodes a message (fingerprint & data) into binary form");
    emit(0, "//");
    emit(0, "// returns Encoded data or error");
    emit(0, "func (p *%s) Encode() (data []byte, err error) {", gotype);
    emit(1, "var size int");
    emit(1, "if size, err = p.Size(); err != nil {");
    emit(2, "return");
    emit(1, "}");
    emit_nl();
    emit(1, "data = make([]byte, 8 + size)");
    emit(1, "binary.BigEndian.PutUint64(data, Fingerprint())");
    emit_nl();
    emit(1, "var d []byte");
    emit(1, "if d, err = p.MarshalBinary(); err != nil {");
    emit(2, "return");
    emit(1, "}");
    emit_nl();
    emit(1, "if copied := copy(data[8:], d); copied != size {");
    emit(2, "return []byte{},");
    emit(3, "fmt.Errorf(\"Encoding error, buffer not filled (%%v != %%v)\", copied, size)");
    emit(1, "}");
    emit(1, "return");
    emit(0, "}");
    emit_nl();
}

/*
 * Emits getters for read-only struct members. That is all who are used as the
 * size definition for LCM dynamic arrays.
 */
static void emit_go_read_only_getters(FILE *f, lcm_struct_t *ls,
    const char *gotype) {

    for (unsigned int k = 0; k < ls->members->len; k++) {
        lcm_member_t *lm = (lcm_member_t *)g_ptr_array_index(ls->members, k);

        unsigned int i = lcm_find_member_with_named_dimension(ls, lm->membername, k);
        if (i >= ls->members->len) {
            continue;
        }

        const char *methodname = go_membername(f, ls, lm->membername, TRUE);
        const char *structname = go_membername(f, ls, lm->membername, FALSE);
        const char *type = map_type_name(lm->type->lctypename);

        emit(0, "// %s returns the value of dynamic array size attribute", methodname);
        emit(0, "// %s.%s.", gotype, lm->membername);
        emit(0, "// And validates that the size is correct for all fields in which it is used.");
        emit(0, "func (p *%s) %s (%s, error) {", gotype, methodname, type);

        unsigned int first = TRUE;

        for (;
            i < ls->members->len;
            i = lcm_find_member_with_named_dimension(ls, lm->membername, i + 1)) {

            lcm_member_t *lm_ = (lcm_member_t *) g_ptr_array_index(ls->members, i);
            const char *membername = go_membername(f, ls, lm_->membername, TRUE);

            unsigned int j = lcm_find_named_dimension(f, ls, lm_, lm->membername, 0);

            for (; j < lm_->dimensions->len;
                j = lcm_find_named_dimension(f, ls, lm_, lm->membername, j + 1)) {

                GString *arraystr = g_string_new(NULL);
                emit_go_array_loops(f, ls, lm_, arraystr, FALSE, j);

                emit(j+1, "// %s%s", membername, arraystr->str);

                if (first) {
                    first = FALSE;
                    emit(1, "// Set value to first dynamic array using this size");
                    emit(1, "p.%s = %s(len(p.%s%s))", structname,
                        map_builtintype_name(lm->type->lctypename),
                        membername, arraystr->str);

                    emit_nl();
                    emit(1, "// Validate size matches all other dynamic arrays");
                } else {
                    emit(j+1, "if int(p.%s) != len(p.%s%s) {", structname, membername, arraystr->str);
                    emit(j+2, "return 0, fmt.Errorf(\"Defined dynamic array size not matching actual\" +");
                    emit(j+3, "\" array size (got %%d expected %%d for %s%s)\",", membername, arraystr->str);
                    emit(j+3, " len(p.%s%s), p.%s)", membername, arraystr->str, structname);
                    emit(j+1, "}");
                }

                g_string_free(arraystr, TRUE);
            }

            emit_go_array_loops_end(f, j-1);
            emit_nl();

            free((char *)membername);
        }

        emit(1, "// Return size");
        emit(1, "return p.%s, nil", structname);
        emit(0, "}");
        emit_nl();

        free((char *)type);
        free((char *)structname);
        free((char *)methodname);
    }
}

/*
 * Emits validation code for a struct member that is used as the size
 * definition for a LCM dynamic array, and populates the read-only member with
 * correct value.
 */
static void emit_go_dynamic_array_check(FILE *f, lcm_struct_t *ls,
    lcm_member_t *lm) {
    if (lcm_find_member_with_named_dimension(ls, lm->membername, 0)
        < ls->members->len) {
        const char *methodname = go_membername(f, ls, lm->membername, TRUE);
        const char *typename = go_membername(f, ls, lm->membername, FALSE);
        emit(1, "// Validate and populate p.%s", typename);
        emit(1, "if _, err = p.%s; err != nil {", methodname);
        emit(2, "return");
        emit(1, "}");
        free((char *)typename);
        free((char *)methodname);
    }
}

/*
 * Emits Golang code to marshal each of the structs members into a byte slice
 * that can be send.
 */
static void emit_go_marshal_binary(FILE *f, lcm_struct_t *ls, const char *gotype) {
    emit(0, "// MarshalBinary implements the BinaryMarshaller interface");
    emit(0, "func (p *%s) MarshalBinary() (data []byte, err error) {", gotype);
    if (ls->members->len) {
        emit(1, "var size int");
        emit(1, "if size, err = p.Size(); err != nil {");
        emit(2, "return");
        emit(1, "}");
        emit_nl();
        emit(1, "data = make([]byte, size)");
        emit(1, "offset := 0");
        emit_nl();
    }

    for (unsigned int i = 0; i < ls->members->len; i++) {
        lcm_member_t *lm = (lcm_member_t *)g_ptr_array_index(ls->members, i);

        emit_comment(f, 1, lm->comment);
        emit(1, "// LCM struct name: %s", lm->membername);

        const char *membername = go_membername(f, ls, lm->membername, FALSE);

        if (lcm_is_primitive_type(lm->type->lctypename)) {
            if (!lm->dimensions->len) {
                emit_encode_function(f, lm->type->lctypename,
                    membername, 1);
            } else {
                GString *arraystr = g_string_new(membername);
                unsigned int n = emit_go_array_loops(f, ls, lm, arraystr, FALSE, lm->dimensions->len);
                emit_encode_function(f, lm->type->lctypename,
                        arraystr->str, 1+n);
                emit_go_array_loops_end(f, n);
                g_string_free(arraystr, TRUE);
            }
        } else {
            if (!lm->dimensions->len) {
                emit(1, "{");
                emit(2, "var tmp []byte");
                emit(2, "tmp, err = p.%s.MarshalBinary(); if err != nil {", membername);
                emit(3, "return");
                emit(2, "}");
                emit(2, "offset += copy(data[offset:], tmp)");
                emit(1, "}");
            } else {
                GString *arraystr = g_string_new(NULL);
                unsigned int n = emit_go_array_loops(f, ls, lm, arraystr, FALSE, lm->dimensions->len);
                emit(2, "var tmp []byte");
                emit(2, "tmp, err = p.%s%s.MarshalBinary(); if err != nil {",
                        membername, arraystr->str);
                emit(3, "return");
                emit(2, "}");
                emit(2, "offset += copy(data[offset:], tmp)");
                emit_go_array_loops_end(f, n);
                g_string_free(arraystr, TRUE);
            }
        }

        emit_nl();
        free((char *)membername);
    }

    emit(1, "return");
    emit(0, "}");
    emit_nl();
}

/*
 * Emits the main decode function.
 */
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
    emit(2, "fp != Fingerprint() {");
    emit(2, "return fmt.Errorf(\"Fingerprints does not match (got %%x expected %%x)\",");
    emit(3, "fp, Fingerprint())");
    emit(1, "}");
    emit_nl();
    emit(1, "if err = p.UnmarshalBinary(data[8:]); err != nil {");
    emit(2, "return");
    emit(1, "}");
    emit_nl();
    emit(1, "length -= 8");
    emit(1, "var size int");
    emit(1, "if size, err = p.Size(); err != nil {");
    emit(2, "return");
    emit(1, "}");
    emit(1, "if length != size {");
    emit(2, "return fmt.Errorf(\"Missing data in buffer (size missmatch, got %%v expected %%v)\",");
    emit(3, "length, size)");
    emit(1, "}");
    emit_nl();
    emit(1, "return");
    emit(0, "}");
    emit_nl();
}

/*
 * Emits Golang code to unmarshal data from a byte slice back into the structs
 * members.
 */
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

        const char *membername = go_membername(f, ls, lm->membername, FALSE);

        if (lcm_is_primitive_type(lm->type->lctypename)) {
            if (!lm->dimensions->len) {
                emit_decode_function(f, lm->type->lctypename, membername, 1);
            } else {
                GString *arraystr = g_string_new(membername);
                unsigned int n = emit_go_array_loops(f, ls, lm, arraystr, TRUE, lm->dimensions->len);
                emit_decode_function(f, lm->type->lctypename, arraystr->str, 1+n);
                emit_go_array_loops_end(f, n);
                g_string_free(arraystr, TRUE);
            }
        } else {
            if (!lm->dimensions->len) {
                emit(1, "err = p.%s.UnmarshalBinary(data[offset:]); if err != nil {", membername);
                emit(2, "return");
                emit(1, "}");
                emit(1, "{");
                emit(2, "var size int");
                emit(2, "if size, err = p.%s.Size(); err != nil {", membername);
                emit(3, "return");
                emit(2, "}");
                emit(2, "offset += size");
                emit(1, "}");
            } else {
                GString *arraystr = g_string_new(membername);
                unsigned int n = emit_go_array_loops(f, ls, lm, arraystr, TRUE, lm->dimensions->len);
                emit(n + 1, "err = p.%s.UnmarshalBinary(data[offset:]); if err != nil {",
                        arraystr->str);
                emit(n + 2, "return");
                emit(n + 1, "}");
                emit(n + 1, "var size int");
                emit(n + 1, "if size, err = p.%s.Size(); err != nil {", arraystr->str);
                emit(n + 2, "return");
                emit(n + 1, "}");
                emit(n + 1, "offset += size");
                emit_go_array_loops_end(f, n);
                g_string_free(arraystr, TRUE);
            }
        }

        emit_nl();
        free((char *)membername);
    }

    emit(1, "return");
    emit(0, "}");
    emit_nl();
}

/*
 * The emitted code generates the fingerprint as described in
 * https://lcm-proj.github.io/type_specification.html
 */
static void emit_go_fingerprint(FILE *f, lcm_struct_t *ls, const char *gotype) {
    emit(0, "// Fingerprint generates the LCM fingerprint value for this message");
    emit(0, "func Fingerprint(path ...uint64) uint64 {");
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
            const char *typename = dots_to_underscores(lm->type->lctypename);
            emit_start(2, "%s.Fingerprint(path...)", typename);
            free((char *)typename);
        }
    }

    emit_end(", 1)");
    emit(0, "}");
    emit_nl();
}

/*
 * Emits code to calculate the size a string in bytes. This takes into account
 * that LCM will send 4 bytes prepending the string and one terminating it.
 */
static void emit_go_string_size(FILE *f, int indent, const char *str_prefix,
                                const char *str_name, const char *str_postfix,
                                const char *rec_val) {
    emit(indent, "%s += 4 // LCM string length", rec_val);
    emit(indent, "%s += len([]byte(%s%s%s))",rec_val, str_prefix, str_name,
         str_postfix);
    emit(indent, "%s += 1 // LCM zero termination", rec_val);
}

/*
 * Emits codes to calculate the actual size in bytes when this message is
 * marshalled.
 */
static void emit_go_size(FILE *f, lcm_struct_t *ls, const char *gotype) {
    emit(0, "// Size returns the size of this message in bytes");
    emit(0, "func (p *%s) Size() (size int, err error) {", gotype);
    emit_nl();

    if (!ls->members->len) {
        goto ret_size;
    }

    for (unsigned int m = 0; m < ls->members->len; m++) {
        lcm_member_t *lm = (lcm_member_t *)g_ptr_array_index(ls->members, m);

        const char *membername = go_membername(f, ls, lm->membername, FALSE);

        if (lcm_is_primitive_type(lm->type->lctypename)) {
            if (!lm->dimensions->len) {
                if (strcmp(lm->type->lctypename, "string") == 0) {
                    emit_go_string_size(f, 1, "p.", membername, "", "size");
                } else {
                    emit_go_dynamic_array_check(f, ls, lm);
                    emit(1, "size += %d // p.%s",
                         primitive_type_size(lm->type->lctypename), membername);
                }
            } else {
                GString *arraystr = g_string_new(NULL);
                unsigned int n = emit_go_array_loops(f, ls, lm, arraystr, FALSE, lm->dimensions->len);

                if (strcmp(lm->type->lctypename, "string") == 0) {
                    emit_go_string_size(f, n + 1, "p.", membername,
                                        arraystr->str, "size");
                } else {
                    emit(n + 1, "size += %d // p.%s",
                         primitive_type_size(lm->type->lctypename), membername);
                }

                emit_go_array_loops_end(f, n);
                g_string_free(arraystr, TRUE);
            }
        } else {
            if (!lm->dimensions->len) {
                emit(1, "{");
                emit(2, "var tmp int");
                emit(2, "if tmp, err = p.%s.Size(); err != nil {", membername);
                emit(3, "return");
                emit(2, "}");
                emit(2, "size += tmp");
                emit(1, "}");
            } else {
                GString *arraystr = g_string_new(NULL);
                unsigned int n = emit_go_array_loops(f, ls, lm, arraystr, FALSE, lm->dimensions->len);
                emit(n + 1, "var tmp int");
                emit(n + 1, "if tmp, err = p.%s%s.Size(); err != nil {", membername, arraystr->str);
                emit(n + 2, "return");
                emit(n + 1, "}");
                emit(n + 1, "size += tmp");
                emit_go_array_loops_end(f, n);
                g_string_free(arraystr, TRUE);
            }
        }

        free((char *)membername);

        emit_nl();
    }

ret_size:
    emit(1, "return");
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

    // Fingerprint const
    emit_go_fingerprint_const(f, ls);

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

    // Getters for read only fields (all fields used as array sizes)
    emit_go_read_only_getters(f, ls, gotype);

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
