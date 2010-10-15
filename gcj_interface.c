
#include "gcj_interface.h"
#include "types.h"
#include "class_file.h"

#include <libfirm/firm.h>

#include <assert.h>
#include <string.h>

static ident     *class_dollar_ident;
static ir_type   *glob;
static ir_entity *gcj_alloc_entity;
static ir_entity *gcj_init_entity;
static ir_entity *gcj_new_string_entity;
static ir_entity *gcj_new_prim_array_entity;
static ir_entity *gcj_new_object_array_entity;
static ir_entity *gcj_abstract_method_entity;
static ir_entity *gcj_lookup_interface_entity;

static ir_entity *gcj_byteClass_entity;
static ir_entity *gcj_charClass_entity;
static ir_entity *gcj_shortClass_entity;
static ir_entity *gcj_intClass_entity;
static ir_entity *gcj_longClass_entity;
static ir_entity *gcj_floatClass_entity;
static ir_entity *gcj_doubleClass_entity;
static ir_entity *gcj_compiled_execution_engine_entity;

static ir_mode   *mode_ushort;
static ir_type   *type_ushort;

extern ir_entity *vptr_entity;

static ir_entity *add_class_dollar_field_recursive(ir_type *type)
{
	assert (is_Class_type(type));

	ir_entity *superclass_class_dollar_field = NULL;

	int n_supertypes = get_class_n_supertypes(type);
	if (n_supertypes > 0) {
		assert(n_supertypes == 1);
		ir_type *superclass = get_class_supertype(type, 0);
		superclass_class_dollar_field = get_class_member_by_name(superclass, class_dollar_ident);
		if (superclass_class_dollar_field == NULL) {
			superclass_class_dollar_field = add_class_dollar_field_recursive(superclass);
		}
	}

	ir_entity *class_dollar_field = new_entity(type, class_dollar_ident, type_reference);
	if (superclass_class_dollar_field != NULL) {
		add_entity_overwrites(class_dollar_field, superclass_class_dollar_field);
	}
	ident *mangled_id = mangle_entity_name(class_dollar_field, class_dollar_ident);
	set_entity_ld_ident(class_dollar_field, mangled_id);

	return class_dollar_field;
}

int gcji_is_api_class(ir_type *type)
{
	assert (is_Class_type(type));
	const char *classname = get_class_name(type);
	return strncmp("java/", classname, 5)  == 0
		|| strncmp("javax/", classname, 6) == 0
		|| strncmp("gnu/", classname, 4)   == 0
		|| strncmp("sun/", classname, 4)   == 0;
}

void gcji_init()
{
	class_dollar_ident = new_id_from_str("class$");
	glob               = get_glob_type();

	ir_type *t_ptr     = new_type_primitive(mode_reference);
	ir_type *t_size    = new_type_primitive(mode_Iu);

	// gcj_alloc
	ir_type *gcj_alloc_method_type = new_type_method(1, 1);
	set_method_param_type(gcj_alloc_method_type, 0, t_ptr);
	set_method_res_type(gcj_alloc_method_type, 0, t_ptr);
	set_method_additional_property(gcj_alloc_method_type, mtp_property_malloc);

	ident   *gcj_alloc_id = new_id_from_str("_Jv_AllocObjectNoInitNoFinalizer");
	gcj_alloc_entity = new_entity(glob, gcj_alloc_id, gcj_alloc_method_type);
	set_entity_visibility(gcj_alloc_entity, ir_visibility_external);

	// gcj_init
	ir_type *gcj_init_method_type = new_type_method(1, 0);
	set_method_param_type(gcj_init_method_type, 0, t_ptr);

	ident   *gcj_init_id = new_id_from_str("_Jv_InitClass");
	gcj_init_entity = new_entity(glob, gcj_init_id, gcj_init_method_type);
	set_entity_visibility(gcj_init_entity, ir_visibility_external);

	// gcj_new_string
	ir_type *gcj_new_string_method_type = new_type_method(1, 1);
	set_method_param_type(gcj_new_string_method_type, 0, t_ptr);
	set_method_res_type(gcj_new_string_method_type, 0, t_ptr);

	ident   *gcj_new_string_id = new_id_from_str("_Jv_NewStringUTF");
	gcj_new_string_entity = new_entity(glob, gcj_new_string_id, gcj_new_string_method_type);
	set_entity_visibility(gcj_new_string_entity, ir_visibility_external);

	// gcj_new_prim_array
	ir_type *gcj_new_prim_array_method_type = new_type_method(2, 1);
	set_method_param_type(gcj_new_prim_array_method_type, 0, t_ptr);
	set_method_param_type(gcj_new_prim_array_method_type, 1, t_size);
	set_method_res_type(gcj_new_prim_array_method_type, 0, t_ptr);

	ident   *gcj_new_prim_array_id = new_id_from_str("_Jv_NewPrimArray");
	gcj_new_prim_array_entity = new_entity(glob, gcj_new_prim_array_id, gcj_new_prim_array_method_type);
	set_entity_visibility(gcj_new_prim_array_entity, ir_visibility_external);

	// gcj_new_object_array
	ir_type *gcj_new_object_array_method_type = new_type_method(3, 1);
	set_method_param_type(gcj_new_object_array_method_type, 0, t_size);
	set_method_param_type(gcj_new_object_array_method_type, 1, t_ptr);
	set_method_param_type(gcj_new_object_array_method_type, 2, t_ptr);
	set_method_res_type(gcj_new_object_array_method_type, 0, t_ptr);

	ident   *gcj_new_object_array_id = new_id_from_str("_Jv_NewObjectArray");
	gcj_new_object_array_entity = new_entity(glob, gcj_new_object_array_id, gcj_new_object_array_method_type);
	set_entity_visibility(gcj_new_object_array_entity, ir_visibility_external);

	// gcji_abstract_method
	ir_type *gcj_abstract_method_type = new_type_method(0, 0);
	gcj_abstract_method_entity = new_entity(glob, new_id_from_str("_Jv_ThrowAbstractMethodError"), gcj_abstract_method_type);
	set_entity_visibility(gcj_abstract_method_entity, ir_visibility_external);

	// gcji_lookup_interface
	ir_type *gcj_lookup_interface_type = new_type_method(3,1);
	set_method_param_type(gcj_lookup_interface_type, 0, t_ptr);
	set_method_param_type(gcj_lookup_interface_type, 1, t_ptr);
	set_method_param_type(gcj_lookup_interface_type, 2, t_ptr);
	set_method_res_type(gcj_lookup_interface_type, 0, t_ptr);
	gcj_lookup_interface_entity = new_entity(glob, new_id_from_str("_Jv_LookupInterfaceMethod"), gcj_lookup_interface_type);
	set_entity_visibility(gcj_lookup_interface_entity, ir_visibility_external);

	// primitive classes
	gcj_byteClass_entity   = new_entity(glob, new_id_from_str("_Jv_byteClass"), type_reference);
	gcj_charClass_entity   = new_entity(glob, new_id_from_str("_Jv_charClass"), type_reference);
	gcj_shortClass_entity  = new_entity(glob, new_id_from_str("_Jv_shortClass"), type_reference);
	gcj_intClass_entity    = new_entity(glob, new_id_from_str("_Jv_intClass"), type_reference);
	gcj_longClass_entity   = new_entity(glob, new_id_from_str("_Jv_longClass"), type_reference);
	gcj_floatClass_entity  = new_entity(glob, new_id_from_str("_Jv_floatClass"), type_reference);
	gcj_doubleClass_entity = new_entity(glob, new_id_from_str("_Jv_doubleClass"), type_reference);
	set_entity_visibility(gcj_byteClass_entity, ir_visibility_external);
	set_entity_visibility(gcj_charClass_entity, ir_visibility_external);
	set_entity_visibility(gcj_shortClass_entity, ir_visibility_external);
	set_entity_visibility(gcj_intClass_entity, ir_visibility_external);
	set_entity_visibility(gcj_longClass_entity, ir_visibility_external);
	set_entity_visibility(gcj_floatClass_entity, ir_visibility_external);
	set_entity_visibility(gcj_doubleClass_entity, ir_visibility_external);

	// execution engine
	gcj_compiled_execution_engine_entity = new_entity(glob, new_id_from_str("_Jv_soleCompiledEngine"), type_reference);
	set_entity_visibility(gcj_compiled_execution_engine_entity, ir_visibility_external);

	mode_ushort = new_ir_mode("US", irms_int_number, 16, 0, irma_twos_complement, 16);
	type_ushort = new_type_primitive(mode_ushort);
}

void gcji_class_init(ir_type *type, ir_graph *irg, ir_node *block, ir_node **mem)
{
	assert (is_Class_type(type));
	assert (gcji_is_api_class(type));

	symconst_symbol init_sym;
	init_sym.entity_p = gcj_init_entity;
	ir_node *init_callee = new_r_SymConst(irg, mode_reference, init_sym, symconst_addr_ent);

	ir_entity *class_dollar_field = gcji_get_class_dollar_field(type);
	assert (class_dollar_field);

	symconst_symbol class_dollar_sym;
	class_dollar_sym.entity_p   = class_dollar_field;
	ir_node *class_dollar_symc = new_r_SymConst(irg, mode_reference, class_dollar_sym, symconst_addr_ent);

	ir_node *init_args[1]     = { class_dollar_symc };
	ir_type *init_call_type   = get_entity_type(gcj_init_entity);
	ir_node *init_call        = new_r_Call(block, *mem, init_callee, 1, init_args, init_call_type);
	        *mem              = new_r_Proj(init_call, mode_M, pn_Call_M);
}

ir_node *gcji_allocate_object(ir_type *type, ir_graph *irg, ir_node *block, ir_node **mem)
{
	assert (is_Class_type(type));

	ir_node *cur_mem = *mem;
	if (gcji_is_api_class(type))
	  gcji_class_init(type, irg, block, &cur_mem);

	symconst_symbol alloc_sym;
	alloc_sym.entity_p = gcj_alloc_entity;
	ir_node *alloc_callee = new_r_SymConst(irg, mode_reference, alloc_sym, symconst_addr_ent);

	ir_entity *class_dollar_field = gcji_get_class_dollar_field(type);
	assert (class_dollar_field);

	symconst_symbol class_dollar_sym;
	class_dollar_sym.entity_p   = class_dollar_field;
	ir_node *class_dollar_symc = new_r_SymConst(irg, mode_reference, class_dollar_sym, symconst_addr_ent);

	ir_node *alloc_args[1]   = { class_dollar_symc };
	ir_type *alloc_call_type = get_entity_type(gcj_alloc_entity);
	ir_node *alloc_call      = new_r_Call(block, cur_mem, alloc_callee, 1, alloc_args, alloc_call_type);
             cur_mem         = new_r_Proj(alloc_call, mode_M, pn_Call_M);
	ir_node *ress            = new_r_Proj(alloc_call, mode_T, pn_Call_T_result);
	ir_node *res             = new_r_Proj(ress, mode_reference, 0);

	*mem = cur_mem;
	return res;
}

ir_node *gcji_allocate_array(ir_type *eltype, ir_node *count, ir_graph *irg, ir_node *block, ir_node **mem)
{
	ir_node   *res        = NULL;
	ir_node   *cur_mem    = *mem;

	if (is_Primitive_type(eltype)) {
		ir_entity *jclass_ref = NULL;

		     if (eltype == type_byte)   jclass_ref = gcj_byteClass_entity;
		else if (eltype == type_char)   jclass_ref = gcj_charClass_entity;
		else if (eltype == type_short)  jclass_ref = gcj_shortClass_entity;
		else if (eltype == type_int)    jclass_ref = gcj_intClass_entity;
		else if (eltype == type_long)   jclass_ref = gcj_longClass_entity;
		else if (eltype == type_float)  jclass_ref = gcj_floatClass_entity;
		else if (eltype == type_double) jclass_ref = gcj_doubleClass_entity;
		else assert (0);

		symconst_symbol callee_sym;
		callee_sym.entity_p = gcj_new_prim_array_entity;
		ir_node *callee = new_r_SymConst(irg, mode_reference, callee_sym, symconst_addr_ent);

		symconst_symbol jclass_sym;
		jclass_sym.entity_p = jclass_ref;
		ir_node *jclass = new_r_SymConst(irg, mode_reference, jclass_sym, symconst_addr_ent);

		ir_node *args[2] = { jclass, count };


		ir_type *callee_type = get_entity_type(gcj_new_prim_array_entity);
		ir_node *call        = new_r_Call(block, cur_mem, callee, 2, args, callee_type);
		         cur_mem     = new_r_Proj(call, mode_M, pn_Call_M);
		ir_node *ress        = new_r_Proj(call, mode_T, pn_Call_T_result);
		         res         = new_r_Proj(ress, mode_reference, 0);
	} else {
		assert (is_Class_type(eltype));

		ir_entity *class_dollar_field = gcji_get_class_dollar_field(eltype);
		assert (class_dollar_field);

		symconst_symbol callee_sym;
		callee_sym.entity_p = gcj_new_object_array_entity;
		ir_node *callee = new_r_SymConst(irg, mode_reference, callee_sym, symconst_addr_ent);

		symconst_symbol jclass_sym;
		jclass_sym.entity_p = class_dollar_field;
		ir_node *jclass = new_r_SymConst(irg, mode_reference, jclass_sym, symconst_addr_ent);

		ir_node *nullptr = new_Const_long(mode_reference, 0);

		ir_node *args[3] = { count, jclass, nullptr };


		ir_type *callee_type = get_entity_type(gcj_new_object_array_entity);
		ir_node *call        = new_r_Call(block, cur_mem, callee, 3, args, callee_type);
		         cur_mem     = new_r_Proj(call, mode_M, pn_Call_M);
		ir_node *ress        = new_r_Proj(call, mode_T, pn_Call_T_result);
		         res         = new_r_Proj(ress, mode_reference, 0);
	}

	*mem = cur_mem;
	return res;
}

ir_node *gcji_new_string(ir_entity *bytes, ir_graph *irg, ir_node *block, ir_node **mem)
{
	symconst_symbol callee_sym;
	callee_sym.entity_p = gcj_new_string_entity;
	ir_node *callee = new_r_SymConst(irg, mode_reference, callee_sym, symconst_addr_ent);

	symconst_symbol string_sym;
	string_sym.entity_p = bytes;
	ir_node *string_symc = new_r_SymConst(irg, mode_reference, string_sym, symconst_addr_ent);

	ir_node *args[1] = { string_symc };

	ir_node *cur_mem     = *mem;
	ir_type *callee_type = get_entity_type(gcj_new_string_entity);
	ir_node *call        = new_r_Call(block, cur_mem, callee, 1, args, callee_type);
	         cur_mem     = new_r_Proj(call, mode_M, pn_Call_M);
	ir_node *ress        = new_r_Proj(call, mode_T, pn_Call_T_result);
	ir_node *res         = new_r_Proj(ress, mode_reference, 0);

	*mem = cur_mem;
	return res;
}

ir_node *gcji_get_arraylength(ir_node *arrayref, ir_graph *irg, ir_node *block, ir_node **mem)
{
	(void) irg;
	ir_node *cur_mem = *mem;

	ir_node *length_offset = new_Const_long(mode_reference, GCJI_LENGTH_OFFSET); // in gcj, arrays are subclasses of java/lang/Object. "length" is the second field.
	ir_node *length_addr = new_r_Add(block, arrayref, length_offset, mode_reference);
	ir_node *length_load = new_r_Load(block, cur_mem, length_addr, mode_int, cons_none);
	         cur_mem     = new_r_Proj(length_load, mode_M, pn_Load_M);
	ir_node *res         = new_r_Proj(length_load, mode_int, pn_Load_res);

	*mem = cur_mem;
	return res;
}

static ir_node *create_symconst(ir_graph *irg, ir_entity *ent)
{
	symconst_symbol sym;
	sym.entity_p = ent;
	ir_node *symc = new_r_SymConst(irg, mode_reference, sym, symconst_addr_ent);
	return symc;
}

static ir_node *create_ccode_symconst(ir_entity *ent)
{
	ir_graph *ccode = get_const_code_irg();
	return create_symconst(ccode, ent);
}

static ir_entity *emit_primitive_member(ir_type *owner, const char *name, ir_type *type, ir_node *value)
{
	assert (is_Primitive_type(type));
	ident            *id   = new_id_from_str(name);
	ir_entity        *ent  = new_entity(owner, id, type);
	ir_initializer_t *init = create_initializer_const(value);
	set_entity_initializer(ent, init);
	return ent;
}

extern char* strdup(const char* s);

static ir_entity *emit_utf8_const(constant_t *constant, int mangle_slash)
{
	assert (constant->base.kind == CONSTANT_UTF8_STRING);
	constant_utf8_string_t *string = (constant_utf8_string_t*) constant;

	char    *bytes = mangle_slash ? strdup(string->bytes) : string->bytes;
	uint16_t len   = string->length;
	uint16_t len0  = string->length + 1; // include the '\0' byte

	if (mangle_slash)
	  for (char *p = bytes; *p != '\0'; p++)
		if (*p == '/') *p = '.';

	int hash = 0;
	for (uint16_t i = 0; i < len; i++) {
		hash = (31 * hash) + bytes[i]; // FIXME: this doesn't work for codepoints that are not ASCII.
	}
	hash &= 0xFFFF;

	ir_graph         *ccode      = get_const_code_irg();

	ident            *id         = id_unique("_Utf8_%u_");
	ir_type          *utf8c_type = new_type_struct(id_mangle(id, new_id_from_str("type")));
	ir_initializer_t *cinit      = create_initializer_compound(3);

	// hash
	ir_entity        *hash_ent   = new_entity(utf8c_type, new_id_from_str("hash"), type_ushort);
	ir_initializer_t *hash_init  = create_initializer_const(new_r_Const_long(ccode, mode_ushort, hash));
	set_entity_initializer(hash_ent, hash_init);
	set_initializer_compound_value(cinit, 0, hash_init);

	// length
	ir_entity        *length_ent = new_entity(utf8c_type, new_id_from_str("length"), type_ushort);
	ir_initializer_t *length_init= create_initializer_const(new_r_Const_long(ccode, mode_ushort, len));
	set_entity_initializer(length_ent, length_init);
	set_initializer_compound_value(cinit, 1, length_init);

	// data
	ir_mode          *el_mode    = mode_Bu;
	ir_type          *el_type    = new_type_primitive(el_mode);
	ir_type          *array_type = new_type_array(1, el_type);

	set_array_lower_bound_int(array_type, 0, 0);
	set_array_upper_bound_int(array_type, 0, len0);
	set_type_size_bytes(array_type, len0);
	set_type_state(array_type, layout_fixed);

	ir_entity        *data_ent   = new_entity(utf8c_type, new_id_from_str("data"), array_type);

	// initialize each array element to an input byte
	ir_initializer_t *data_init  = create_initializer_compound(len0);
	for (size_t i = 0; i < len0; ++i) {
		tarval           *tv  = new_tarval_from_long(bytes[i], el_mode);
		ir_initializer_t *val = create_initializer_tarval(tv);
		set_initializer_compound_value(data_init, i, val);
	}
	set_entity_initializer(data_ent, data_init);
	set_initializer_compound_value(cinit, 2, data_init);

	set_type_size_bytes(utf8c_type, get_type_size_bytes(type_ushort)*2 + get_type_size_bytes(array_type));
	default_layout_compound_type(utf8c_type);

	// finally, the entity for the utf8 constant
	ir_entity        *utf8c   = new_entity(get_glob_type(), id, utf8c_type);
	set_entity_initializer(utf8c, cinit);
	set_entity_allocation(utf8c, allocation_static);
	set_entity_ld_ident(utf8c, id);

	return utf8c;
}
#define MD_SIZE_BYTES (get_type_size_bytes(type_reference)*4 + get_type_size_bytes(type_ushort)*2)
static ir_entity *emit_method_desc(ir_type *owner, ir_type *classtype, ir_entity *ent)
{
	ir_graph         *ccode         = get_const_code_irg();

	ident            *id            = id_unique("_MD_%u_");
	ir_type          *md_type       = new_type_struct(id_mangle(id, new_id_from_str("type")));

	ir_initializer_t *cinit         = create_initializer_compound(6);

	class_t          *linked_class  = (class_t*) get_type_link(classtype);
	method_t         *linked_method = (method_t*) get_entity_link(ent);
	assert (linked_class && linked_method);

	constant_t       *name_const    = linked_class->constants[linked_method->name_index];
	ir_entity        *name_const_ent= emit_utf8_const(name_const, 1);
	ir_entity        *name_ent      = emit_primitive_member(md_type, "name", type_reference, create_ccode_symconst(name_const_ent));
	set_initializer_compound_value(cinit, 0, get_entity_initializer(name_ent));

	constant_t       *desc_const    = linked_class->constants[linked_method->descriptor_index];
	ir_entity        *desc_const_ent= emit_utf8_const(desc_const, 1);
	ir_entity        *desc_ent      = emit_primitive_member(md_type, "sig", type_reference, create_ccode_symconst(desc_const_ent));
	set_initializer_compound_value(cinit, 1, get_entity_initializer(desc_ent));

	ir_node          *accflags      = new_r_Const_long(ccode, mode_ushort, linked_method->access_flags | 0x4000); // 0x4000 is gcj specific, meaning ACC_TRANSLATED.
	ir_entity        *accflags_ent  = emit_primitive_member(md_type, "accflags", type_ushort, accflags);
	set_initializer_compound_value(cinit, 2, get_entity_initializer(accflags_ent));

	ir_node          *index         = new_r_Const_long(ccode, mode_ushort, get_entity_vtable_number(ent));
	ir_entity        *index_ent     = emit_primitive_member(md_type, "index", type_ushort, index);
	set_initializer_compound_value(cinit, 3, get_entity_initializer(index_ent));


	ir_node          *funcptr       = NULL;
	if ((linked_method->access_flags & ACCESS_FLAG_ABSTRACT) != 0)
		funcptr = create_ccode_symconst(gcj_abstract_method_entity);
	else
		funcptr = create_ccode_symconst(ent);

	ir_entity        *funcptr_ent   = emit_primitive_member(md_type, "ncode", type_reference, funcptr);
	set_initializer_compound_value(cinit, 4, get_entity_initializer(funcptr_ent));

	ir_node          *nullptr       = new_r_Const_long(ccode, mode_reference, 0);
	ir_entity        *throws_ent    = emit_primitive_member(md_type, "throws", type_reference, nullptr);
	set_initializer_compound_value(cinit, 5, get_entity_initializer(throws_ent));

	set_type_size_bytes(md_type, MD_SIZE_BYTES);
	default_layout_compound_type(md_type);

	ir_entity        *md_ent        = new_entity(owner, id, md_type);
	set_entity_initializer(md_ent, cinit);
	set_entity_allocation(md_ent, allocation_static);
	set_entity_ld_ident(md_ent, id);

	return md_ent;
}

static ir_entity *emit_method_table(ir_type *classtype)
{
	ident            *id            = id_unique("_MT_%u_");
	ir_type          *mt_type       = new_type_struct(id_mangle(id, new_id_from_str("type")));

	class_t          *linked_class  = (class_t*) get_type_link(classtype);
	assert (linked_class);

	int               n_members     = get_class_n_members(classtype);
	uint16_t          n_methods     = linked_class->n_methods;
	ir_initializer_t *cinit         = create_initializer_compound(n_methods);
	unsigned          cur_init_slot = 0;

	for (int i = 0; i < n_members; i++) {
		ir_entity *member = get_class_member(classtype, i);
		if (is_method_entity(member)) {
			ir_entity *md_ent = emit_method_desc(mt_type, classtype, member);
			set_initializer_compound_value(cinit, cur_init_slot++, get_entity_initializer(md_ent));
		}
	}
	assert (cur_init_slot == n_methods);

	set_type_size_bytes(mt_type, n_methods * MD_SIZE_BYTES);
	default_layout_compound_type(mt_type);

	ir_entity        *mt_ent        = new_entity(get_glob_type(), id, mt_type);
	set_entity_initializer(mt_ent, cinit);
	set_entity_allocation(mt_ent, allocation_static);
	set_entity_ld_ident(mt_ent, id);

	return mt_ent;
}

#define NUM_FIELDS 39
#define EMIT_PRIM(name, tp, val) do { \
	  ir_entity *ent = emit_primitive_member(cur_cdtype, name, tp, val); \
	  ir_initializer_t *init = get_entity_initializer(ent); \
	  set_initializer_compound_value(cur_init, cur_init_slot++, init); \
	  ir_type *tp = get_entity_type(ent); cur_type_size += get_type_size_bytes(tp); \
	} while(0);

ir_entity *gcji_construct_class_dollar_field(ir_type *classtype)
{
	ir_graph *ccode = get_const_code_irg();
	ir_node *nullref = new_r_Const_long(ccode, mode_reference, 0);
	ir_node *deadref = new_r_Const_long(ccode, mode_reference, 0xdeadbeef);

	ir_type *cur_cdtype = new_type_class(id_mangle_dot(get_class_ident(classtype), class_dollar_ident));
	ir_initializer_t *cur_init = create_initializer_compound(NUM_FIELDS);
	unsigned cur_init_slot = 0;
	unsigned cur_type_size = 0;

	class_t *linked_class = (class_t*)get_type_link(classtype);
	assert (linked_class);

	EMIT_PRIM("next_or_version", type_reference, nullref);
	EMIT_PRIM("unknown", type_int, new_r_Const_long(ccode, mode_int, 404000));

	ir_entity *name_ent = emit_utf8_const(
			linked_class->constants[linked_class->constants[linked_class->this_class]->classref.name_index], 1);
	EMIT_PRIM("name", type_reference, create_ccode_symconst(name_ent));

	EMIT_PRIM("acc_flags", type_ushort, new_r_Const_long(ccode, mode_ushort, linked_class->access_flags));

	assert (get_class_n_supertypes(classtype) == 1);
	ir_type *superclass = get_class_supertype(classtype, 0);
	ir_entity *sccdf = get_class_member_by_name(superclass, class_dollar_ident);
	assert (sccdf);
	EMIT_PRIM("superclass", type_reference, create_ccode_symconst(sccdf));

	// _Jv_Constants inlined. not sure if this is needed for compiled code.
	EMIT_PRIM("constants.size", type_int, new_r_Const_long(ccode, mode_int, 0));
	EMIT_PRIM("constants.tags", type_reference, deadref);
	EMIT_PRIM("constants.data", type_reference, deadref);

	ir_entity *mt_ent = emit_method_table(classtype);
	EMIT_PRIM("methods", type_reference, create_ccode_symconst(mt_ent)); // FIXME: union, alternative would be the element type in case classtype is an array.
	EMIT_PRIM("method_count", type_short, new_r_Const_long(ccode, mode_short, linked_class->n_methods));
	EMIT_PRIM("vtable_method_count", type_short, new_r_Const_long(ccode, mode_short, get_class_vtable_size(classtype)-2)); // w/o slots 0=class$ and 1=some_num. see lower_oo.c

	EMIT_PRIM("fields", type_reference, nullref);

	symconst_symbol size_sym;
	size_sym.type_p = classtype;
	EMIT_PRIM("size_in_bytes", type_int, new_r_SymConst(ccode, mode_int, size_sym, symconst_type_size)); // FIXME: maybe there's an offset needed here
	EMIT_PRIM("field_count", type_short, new_r_Const_long(ccode, mode_short, linked_class->n_fields));
	int16_t n_static_fields = 0;
	for (uint16_t i = 0; i < linked_class->n_fields; i++) {
		if ((linked_class->fields[i]->access_flags & ACCESS_FLAG_STATIC) != 0) n_static_fields++;
	}
	EMIT_PRIM("static_field_count", type_short, new_r_Const_long(ccode, mode_short, n_static_fields));

	ir_node *vtable_ref = NULL;
	if ((linked_class->access_flags & ACCESS_FLAG_INTERFACE) == 0) {
		ir_entity *vtable = get_class_member_by_name(glob, mangle_vtable_name(classtype));
		assert (vtable);
		vtable_ref = create_ccode_symconst(vtable);
		ir_node *block = get_irg_current_block(ccode);
		ir_node *vtable_offset = new_r_Const_long(ccode, mode_reference, get_type_size_bytes(type_reference)*GCJI_VTABLE_OFFSET);
		vtable_ref = new_r_Add(block, vtable_ref, vtable_offset, mode_reference);
	} else {
		vtable_ref = nullref;
	}
	EMIT_PRIM("vtable", type_reference, vtable_ref);
	EMIT_PRIM("otable", type_reference, nullref);
	EMIT_PRIM("otable_syms", type_reference, nullref);
	EMIT_PRIM("atable", type_reference, nullref);
	EMIT_PRIM("atable_syms", type_reference, nullref);
	EMIT_PRIM("itable", type_reference, nullref);
	EMIT_PRIM("itable_syms", type_reference, nullref);
	EMIT_PRIM("catch_classes", type_reference, nullref);

	EMIT_PRIM("interfaces", type_reference, nullref); //TODO: this will be needed to use _Jv_IsAssignable...
	EMIT_PRIM("loader", type_reference, nullref);

	EMIT_PRIM("interface_count", type_short, new_r_Const_long(ccode, mode_short, linked_class->n_interfaces));
	EMIT_PRIM("state", type_byte, new_r_Const_long(ccode, mode_byte, 0));

	EMIT_PRIM("thread", type_reference, nullref);

	int16_t depth = 0; //TODO: this will be needed to use _Jv_IsAssignable...
	EMIT_PRIM("depth", type_short, new_r_Const_long(ccode, mode_short, depth));
	EMIT_PRIM("ancestors", type_reference, nullref);

	EMIT_PRIM("idt", type_reference, nullref); // FIXME: distingiush class / iface

	EMIT_PRIM("arrayclass", type_reference, nullref);
	EMIT_PRIM("protectionDomain", type_reference, nullref);

	EMIT_PRIM("assertion_table", type_reference, nullref);
	EMIT_PRIM("hack_signers", type_reference, nullref);

	EMIT_PRIM("chain", type_reference, nullref);
	EMIT_PRIM("aux_info", type_reference, nullref);

	EMIT_PRIM("engine", type_reference, create_ccode_symconst(gcj_compiled_execution_engine_entity));
	EMIT_PRIM("reflection_data", type_reference, nullref);

	assert (cur_init_slot == NUM_FIELDS);

	set_type_size_bytes(cur_cdtype, cur_type_size);
	default_layout_compound_type(cur_cdtype);

	ir_entity *class_dollar_field = new_entity(classtype, class_dollar_ident, cur_cdtype);
	set_entity_initializer(class_dollar_field, cur_init);
	set_entity_allocation(class_dollar_field, allocation_static);
	ident *mangled_id = mangle_entity_name(class_dollar_field, class_dollar_ident);
	set_entity_ld_ident(class_dollar_field, mangled_id);

	return class_dollar_field;
}

ir_entity *gcji_get_class_dollar_field(ir_type *classtype)
{
	assert (is_Class_type(classtype));
	ir_entity *cdf = get_class_member_by_name(classtype, class_dollar_ident);
	if (!cdf)
	  cdf = add_class_dollar_field_recursive(classtype);
	return cdf;
}

ir_node *gcji_lookup_interface(ir_node *objptr, ir_type *iface, ir_entity *method, ir_graph *irg, ir_node *block, ir_node **mem)
{
	ir_node   *cur_mem       = *mem;

	// we need the reference to the object's class$ field
	// first, dereference the vptr in order to get the vtable address.
	ir_node   *vptr_addr     = new_r_Sel(block, new_NoMem(), objptr, 0, NULL, vptr_entity);
	ir_node   *vptr_load     = new_r_Load(block, cur_mem, vptr_addr, mode_reference, cons_none);
	ir_node   *vtable_addr   = new_r_Proj(vptr_load, mode_reference, pn_Load_res);
	           cur_mem       = new_r_Proj(vptr_load, mode_M, pn_Load_M);

	// second, dereference vtable_addr (it points to the slot where the address of the class$ field is stored).
	ir_node   *cd_load       = new_r_Load(block, cur_mem, vtable_addr, mode_reference, cons_none);
	ir_node   *cd_ref        = new_r_Proj(cd_load, mode_reference, pn_Load_res);
	           cur_mem       = new_r_Proj(cd_load, mode_M, pn_Load_M);

	class_t   *linked_class  = (class_t*) get_type_link(iface);
	method_t  *linked_method = (method_t *) get_entity_link(method);
	assert (linked_class && linked_method);

	constant_t *name_const   = linked_class->constants[linked_method->name_index];
	ir_entity *name_const_ent= emit_utf8_const(name_const, 1);
	ir_node   *name_ref      = create_symconst(irg, name_const_ent);

	constant_t *desc_const   = linked_class->constants[linked_method->descriptor_index];
	ir_entity *desc_const_ent= emit_utf8_const(desc_const, 1);
	ir_node   *desc_ref      = create_symconst(irg, desc_const_ent);

	symconst_symbol callee_sym;
	callee_sym.entity_p      = gcj_lookup_interface_entity;
	ir_node   *callee        = new_r_SymConst(irg, mode_reference, callee_sym, symconst_addr_ent);

	ir_node   *args[3]       = { cd_ref, name_ref, desc_ref };
	ir_type   *call_type     = get_entity_type(gcj_lookup_interface_entity);
	ir_node   *call          = new_r_Call(block, cur_mem, callee, 3, args, call_type);
	           cur_mem       = new_r_Proj(call, mode_M, pn_Call_M);
	ir_node   *ress          = new_r_Proj(call, mode_T, pn_Call_T_result);
	ir_node   *res           = new_r_Proj(ress, mode_reference, 0);

	*mem = cur_mem;

	return res;
}
