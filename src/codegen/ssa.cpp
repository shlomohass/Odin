struct ssaModule;
struct ssaProcedure;
struct ssaBlock;
struct ssaValue;


struct ssaModule {
	CheckerInfo *info;
	BaseTypeSizes sizes;
	gbAllocator allocator;

	String layout;

	Map<ssaValue *> values;  // Key: Entity *
	Map<ssaValue *> members; // Key: String
	i32 global_string_index;
};


struct ssaBlock {
	i32 id;
	AstNode *node;
	Scope *scope;
	String label;
	ssaProcedure *parent;

	gbArray(ssaValue *) instrs;
	gbArray(ssaValue *) values;
};

struct ssaTargetList {
	ssaTargetList *prev;
	ssaBlock *break_;
	ssaBlock *continue_;
	ssaBlock *fallthrough_;
};

struct ssaProcedure {
	ssaModule *module;
	String name;
	Entity *entity;
	Type *type;
	DeclInfo *decl;
	AstNode *type_expr;
	AstNode *body;

	gbArray(ssaBlock *) blocks;
	ssaBlock *curr_block;
	ssaTargetList *target_list;

	gbArray(ssaValue *) anonymous_procedures;
};



#define SSA_INSTR_KINDS \
	SSA_INSTR_KIND(Invalid), \
	SSA_INSTR_KIND(Local), \
	SSA_INSTR_KIND(Store), \
	SSA_INSTR_KIND(Load), \
	SSA_INSTR_KIND(GetElementPtr), \
	SSA_INSTR_KIND(Convert), \
	SSA_INSTR_KIND(Br), \
	SSA_INSTR_KIND(Ret), \
	SSA_INSTR_KIND(Unreachable), \
	SSA_INSTR_KIND(BinaryOp), \
	SSA_INSTR_KIND(Count),

enum ssaInstrKind {
#define SSA_INSTR_KIND(x) GB_JOIN2(ssaInstr_, x)
	SSA_INSTR_KINDS
#undef SSA_INSTR_KIND
};

String const ssa_instr_strings[] = {
#define SSA_INSTR_KIND(x) {cast(u8 *)#x, gb_size_of(#x)-1}
	SSA_INSTR_KINDS
#undef SSA_INSTR_KIND
};

enum ssaConversionKind {
	ssaConversion_Invalid,

	ssaConversion_ZExt,
	ssaConversion_FPExt,
	ssaConversion_FPToUI,
	ssaConversion_FPToSI,
	ssaConversion_UIToFP,
	ssaConversion_SIToFP,
	ssaConversion_PtrToInt,
	ssaConversion_IntToPtr,
	ssaConversion_BitCast,

	ssaConversion_Count,
};

struct ssaInstr {
	ssaInstrKind kind;

	ssaBlock *parent;
	Type *type;
	TokenPos pos;

	union {
		struct {
			Entity *entity;
			Type *type;
		} local;
		struct {
			ssaValue *address;
			ssaValue *value;
		} store;
		struct {
			Type *type;
			ssaValue *address;
		} load;
		struct {
			ssaValue *address;
			Type *    result_type;
			Type *    element_type;
			ssaValue *indices[2];
			isize     index_count;
			b32       inbounds;
		} get_element_ptr;
		struct {
			ssaValue *cond;
			ssaBlock *true_block;
			ssaBlock *false_block;
		} br;
		struct { ssaValue *value; } ret;
		struct {} unreachable;

		struct {
			Type *type;
			Token op;
			ssaValue *left, *right;
		} binary_op;

		struct {
			ssaConversionKind kind;
			ssaValue *value;
			Type *from, *to;
		} conversion;
	};
};


enum ssaValueKind {
	ssaValue_Invalid,

	ssaValue_Constant,
	ssaValue_TypeName,
	ssaValue_Global,
	ssaValue_Param,

	ssaValue_Proc,
	ssaValue_Block,
	ssaValue_Instr,

	ssaValue_Count,
};

struct ssaValue {
	ssaValueKind kind;
	i32 id;

	union {
		struct {
			Type *     type;
			ExactValue value;
		} constant;
		struct {
			Entity *entity;
			Type *  type;
		} type_name;
		struct {
			b32       is_gen;
			Entity *  entity;
			Type *    type;
			ssaValue *value;
		} global;
		struct {
			ssaProcedure *parent;
			Entity *entity;
			Type *  type;
		} param;
		ssaProcedure proc;
		ssaBlock     block;
		ssaInstr     instr;
	};
};

gb_global ssaValue *v_zero    = NULL;
gb_global ssaValue *v_one     = NULL;
gb_global ssaValue *v_zero32  = NULL;
gb_global ssaValue *v_one32   = NULL;
gb_global ssaValue *v_two32   = NULL;

enum ssaLvalueKind {
	ssaLvalue_Blank,
	ssaLvalue_Address,

	ssaLvalue_Count,
};

struct ssaLvalue {
	ssaLvalueKind kind;
	union {
		struct {} blank;
		struct {
			ssaValue *value;
			AstNode *expr;
		} address;
	};
};





ssaLvalue ssa_make_lvalue_address(ssaValue *value, AstNode *expr) {
	ssaLvalue lval = {ssaLvalue_Address};
	lval.address.value = value;
	lval.address.expr = expr;
	return lval;
}


void ssa_module_init(ssaModule *m, Checker *c) {
	m->allocator = gb_heap_allocator();
	m->info = &c->info;
	m->sizes = c->sizes;

	map_init(&m->values,  m->allocator);
	map_init(&m->members, m->allocator);
}

void ssa_module_destroy(ssaModule *m) {
	map_destroy(&m->values);
	map_destroy(&m->members);
}

void ssa_module_add_value(ssaModule *m, Entity *e, ssaValue *v) {
	map_set(&m->values, hash_pointer(e), v);
}


Type *ssa_value_type(ssaValue *value);
void  ssa_value_set_type(ssaValue *value, Type *type);

Type *ssa_instr_type(ssaInstr *instr) {
	switch (instr->kind) {
	case ssaInstr_Local:
		return instr->local.type;
	case ssaInstr_Store:
		return ssa_value_type(instr->store.address);
	case ssaInstr_Load:
		return instr->load.type;
	case ssaInstr_GetElementPtr:
		return instr->get_element_ptr.result_type;
	case ssaInstr_BinaryOp:
		return instr->binary_op.type;
	}
	return NULL;
}

void ssa_instr_set_type(ssaInstr *instr, Type *type) {
	switch (instr->kind) {
	case ssaInstr_Local:
		instr->local.type = type;
		break;
	case ssaInstr_Store:
		ssa_value_set_type(instr->store.value, type);
		break;
	case ssaInstr_Load:
		instr->load.type = type;
		break;
	case ssaInstr_GetElementPtr:
		instr->get_element_ptr.result_type = type;
		break;
	case ssaInstr_BinaryOp:
		instr->binary_op.type = type;
		break;
	}
}

Type *ssa_value_type(ssaValue *value) {
	switch (value->kind) {
	case ssaValue_Constant:
		return value->constant.type;
	case ssaValue_TypeName:
		return value->type_name.type;
	case ssaValue_Global:
		return value->global.type;
	case ssaValue_Param:
		return value->param.type;
	case ssaValue_Proc:
		return value->proc.type;
	case ssaValue_Instr:
		return ssa_instr_type(&value->instr);
	}
	return NULL;
}


void ssa_value_set_type(ssaValue *value, Type *type) {
	switch (value->kind) {
	case ssaValue_TypeName:
		value->type_name.type = type;
		break;
	case ssaValue_Global:
		value->global.type = type;
		break;
	case ssaValue_Proc:
		value->proc.type = type;
		break;
	case ssaValue_Constant:
		value->constant.type = type;
		break;
	case ssaValue_Instr:
		ssa_instr_set_type(&value->instr, type);
		break;
	}
}



ssaValue *ssa_build_expr(ssaProcedure *proc, AstNode *expr);
ssaValue *ssa_build_single_expr(ssaProcedure *proc, AstNode *expr, TypeAndValue *tv);
ssaLvalue ssa_build_addr(ssaProcedure *proc, AstNode *expr);
ssaValue *ssa_emit_conv(ssaProcedure *proc, ssaValue *value, Type *a_type);






ssaValue *ssa_alloc_value(gbAllocator a, ssaValueKind kind) {
	ssaValue *v = gb_alloc_item(a, ssaValue);
	v->kind = kind;
	return v;
}

ssaValue *ssa_alloc_instr(gbAllocator a, ssaInstrKind kind) {
	ssaValue *v = ssa_alloc_value(a, ssaValue_Instr);
	v->instr.kind = kind;
	return v;
}

ssaValue *ssa_make_value_type_name(gbAllocator a, Entity *e) {
	ssaValue *v = ssa_alloc_value(a, ssaValue_TypeName);
	v->type_name.entity = e;
	v->type_name.type = e->type;
	return v;
}

ssaValue *ssa_make_value_global(gbAllocator a, Entity *e, ssaValue *value) {
	ssaValue *v = ssa_alloc_value(a, ssaValue_Global);
	v->global.entity = e;
	v->global.type = e->type;
	v->global.value = value;
	return v;
}
ssaValue *ssa_make_value_param(gbAllocator a, ssaProcedure *parent, Entity *e) {
	ssaValue *v = ssa_alloc_value(a, ssaValue_Param);
	v->param.parent = parent;
	v->param.entity = e;
	v->param.type   = e->type;
	return v;
}


ssaValue *ssa_make_instr_local(ssaProcedure *p, Entity *e) {
	ssaValue *v = ssa_alloc_instr(p->module->allocator, ssaInstr_Local);
	ssaInstr *i = &v->instr;
	i->local.entity = e;
	i->local.type = e->type;
	if (p->curr_block) {
		gb_array_append(p->curr_block->values, v);
	}
	ssa_module_add_value(p->module, e, v);
	return v;
}


ssaValue *ssa_make_instr_store(ssaProcedure *p, ssaValue *address, ssaValue *value) {
	ssaValue *v = ssa_alloc_instr(p->module->allocator, ssaInstr_Store);
	ssaInstr *i = &v->instr;
	i->store.address = address;
	i->store.value = value;
	if (p->curr_block) {
		gb_array_append(p->curr_block->values, v);
	}
	return v;
}

ssaValue *ssa_make_instr_load(ssaProcedure *p, ssaValue *address) {
	ssaValue *v = ssa_alloc_instr(p->module->allocator, ssaInstr_Load);
	ssaInstr *i = &v->instr;
	i->load.address = address;
	i->load.type = ssa_value_type(address);
	if (p->curr_block) {
		gb_array_append(p->curr_block->values, v);
	}
	return v;
}

ssaValue *ssa_make_instr_get_element_ptr(ssaProcedure *p, ssaValue *address,
                                               ssaValue *index0, ssaValue *index1, isize index_count,
                                               b32 inbounds) {
	ssaValue *v = ssa_alloc_instr(p->module->allocator, ssaInstr_GetElementPtr);
	ssaInstr *i = &v->instr;
	i->get_element_ptr.address = address;
	i->get_element_ptr.indices[0]   = index0;
	i->get_element_ptr.indices[1]   = index1;
	i->get_element_ptr.index_count  = index_count;
	i->get_element_ptr.element_type = ssa_value_type(address);
	i->get_element_ptr.inbounds     = inbounds;
	if (p->curr_block) {
		gb_array_append(p->curr_block->values, v);
	}
	return v;
}

ssaValue *ssa_make_instr_binary_op(ssaProcedure *p, Token op, ssaValue *left, ssaValue *right) {
	ssaValue *v = ssa_alloc_instr(p->module->allocator, ssaInstr_BinaryOp);
	ssaInstr *i = &v->instr;
	i->binary_op.op = op;
	i->binary_op.left = left;
	i->binary_op.right = right;
	if (p->curr_block) {
		gb_array_append(p->curr_block->values, v);
	}
	return v;
}

ssaValue *ssa_make_instr_br(ssaProcedure *p, ssaValue *cond, ssaBlock *true_block, ssaBlock *false_block) {
	ssaValue *v = ssa_alloc_instr(p->module->allocator, ssaInstr_Br);
	ssaInstr *i = &v->instr;
	i->br.cond = cond;
	i->br.true_block = true_block;
	i->br.false_block = false_block;
	if (p->curr_block) {
		gb_array_append(p->curr_block->values, v);
	}
	return v;
}

ssaValue *ssa_make_instr_unreachable(ssaProcedure *p) {
	ssaValue *v = ssa_alloc_instr(p->module->allocator, ssaInstr_Unreachable);
	if (p->curr_block) {
		gb_array_append(p->curr_block->values, v);
	}
	return v;
}

ssaValue *ssa_make_instr_ret(ssaProcedure *p, ssaValue *value) {
	ssaValue *v = ssa_alloc_instr(p->module->allocator, ssaInstr_Ret);
	v->instr.ret.value = value;
	if (p->curr_block) {
		gb_array_append(p->curr_block->values, v);
	}
	return v;
}




ssaValue *ssa_make_value_constant(gbAllocator a, Type *type, ExactValue value) {
	ssaValue *v = ssa_alloc_value(a, ssaValue_Constant);
	v->constant.type  = type;
	v->constant.value = value;
	return v;
}

ssaValue *ssa_make_value_procedure(gbAllocator a, Entity *e, DeclInfo *decl, ssaModule *m) {
	ssaValue *v = ssa_alloc_value(a, ssaValue_Proc);
	v->proc.module = m;
	v->proc.entity = e;
	v->proc.type   = e->type;
	v->proc.decl   = decl;
	v->proc.name   = e->token.string;
	return v;
}

ssaValue *ssa_make_value_block(ssaProcedure *proc, AstNode *node, Scope *scope, String label) {
	ssaValue *v = ssa_alloc_value(proc->module->allocator, ssaValue_Block);
	v->block.label  = label;
	v->block.node   = node;
	v->block.scope  = scope;
	v->block.parent = proc;

	gb_array_init(v->block.instrs, gb_heap_allocator());
	gb_array_init(v->block.values, gb_heap_allocator());

	return v;
}



ssaValue *ssa_add_global_string(ssaProcedure *proc, ExactValue value) {
	GB_ASSERT(value.kind == ExactValue_String);
	gbAllocator a = gb_heap_allocator();


	isize max_len = 4+8+1;
	u8 *str = cast(u8 *)gb_alloc_array(a, u8, max_len);
	isize len = gb_snprintf(cast(char *)str, max_len, ".str%x", proc->module->global_string_index);
	proc->module->global_string_index++;

	String name = make_string(str, len-1);
	Token token = {Token_String};
	token.string = name;
	Type *type = &basic_types[Basic_string];
	Entity *entity = make_entity_constant(a, NULL, token, type, value);
	ssaValue *v = ssa_make_value_constant(a, type, value);


	ssaValue *g = ssa_make_value_global(a, entity, v);
	g->global.is_gen = true;

	map_set(&proc->module->values, hash_pointer(entity), g);
	map_set(&proc->module->members, hash_string(name), g);

	return g;
}


b32 ssa_is_blank_ident(AstNode *node) {
	if (node->kind == AstNode_Ident) {
		ast_node(i, Ident, node);
		return are_strings_equal(i->token.string, make_string("_"));
	}
	return false;
}



ssaValue *ssa_emit(ssaProcedure *proc, ssaValue *instr) {
	ssaBlock *b = proc->curr_block;
	instr->instr.parent = b;
	if (b) {
		gb_array_append(b->instrs, instr);
	}
	return instr;
}
ssaValue *ssa_emit_store(ssaProcedure *p, ssaValue *address, ssaValue *value) {
	return ssa_emit(p, ssa_make_instr_store(p, address, value));
}
ssaValue *ssa_emit_load(ssaProcedure *p, ssaValue *address) {
	return ssa_emit(p, ssa_make_instr_load(p, address));
}



ssaValue *ssa_add_local(ssaProcedure *proc, Entity *e) {
	return ssa_emit(proc, ssa_make_instr_local(proc, e));
}

ssaValue *ssa_add_local_for_identifier(ssaProcedure *proc, AstNode *name) {
	Entity **found = map_get(&proc->module->info->definitions, hash_pointer(name));
	if (found) {
		return ssa_add_local(proc, *found);
	}
	return NULL;
}

ssaValue *ssa_add_param(ssaProcedure *proc, Entity *e) {
	ssaValue *v = ssa_make_value_param(proc->module->allocator, proc, e);
	ssaValue *l = ssa_add_local(proc, e);
	ssa_emit_store(proc, l, v);
	return v;
}


ssaValue *ssa_lvalue_store(ssaLvalue lval, ssaProcedure *p, ssaValue *value) {
	switch (lval.kind) {
	case ssaLvalue_Address:
		return ssa_emit_store(p, lval.address.value, value);
	}
	return NULL;
}

ssaValue *ssa_lvalue_load(ssaLvalue lval, ssaProcedure *p) {
	switch (lval.kind) {
	case ssaLvalue_Address:
		return ssa_emit_load(p, lval.address.value);
	}
	GB_PANIC("Illegal lvalue load");
	return NULL;
}


ssaValue *ssa_lvalue_address(ssaLvalue lval, ssaProcedure *p) {
	switch (lval.kind) {
	case ssaLvalue_Address:
		return lval.address.value;
	}
	return NULL;
}

Type *ssa_lvalue_type(ssaLvalue lval) {
	switch (lval.kind) {
	case ssaLvalue_Address:
		return type_deref(ssa_value_type(lval.address.value));
	}
	return NULL;
}


void ssa_build_stmt(ssaProcedure *proc, AstNode *s);

void ssa_emit_defer_stmts(ssaProcedure *proc, ssaBlock *block) {
	if (block == NULL)
		return;

	// IMPORTANT TODO(bill): ssa defer - Place where needed!!!

	Scope *curr_scope = block->scope;
	if (curr_scope == NULL) {
		GB_PANIC("No scope found for deferred statements");
	}

	for (Scope *s = curr_scope; s != NULL; s = s->parent) {
		isize count = gb_array_count(s->deferred_stmts);
		for (isize i = count-1; i >= 0; i--) {
			ssa_build_stmt(proc, s->deferred_stmts[i]);
		}
	}
}

void ssa_emit_unreachable(ssaProcedure *proc) {
	ssa_emit(proc, ssa_make_instr_unreachable(proc));
}

void ssa_emit_ret(ssaProcedure *proc, ssaValue *v) {
	ssa_emit_defer_stmts(proc, proc->curr_block);
	ssa_emit(proc, ssa_make_instr_ret(proc, v));
}

void ssa_emit_jump(ssaProcedure *proc, ssaBlock *block) {
	ssa_emit(proc, ssa_make_instr_br(proc, NULL, block, NULL));
	proc->curr_block = NULL;
}

void ssa_emit_if(ssaProcedure *proc, ssaValue *cond, ssaBlock *true_block, ssaBlock *false_block) {
	ssaValue *br = ssa_make_instr_br(proc, cond, true_block, false_block);
	ssa_emit(proc, br);
	proc->curr_block = NULL;
}




ssaBlock *ssa__make_block(ssaProcedure *proc, AstNode *node, String label) {
	Scope *scope = NULL;
	Scope **found = map_get(&proc->module->info->scopes, hash_pointer(node));
	if (found) {
		scope = *found;
	} else {
		GB_PANIC("Block scope not found");
	}

	ssaValue *block = ssa_make_value_block(proc, node, scope, label);
	return &block->block;
}

ssaBlock *ssa_add_block(ssaProcedure *proc, AstNode *node, String label) {
	ssaBlock *block = ssa__make_block(proc, node, label);
	gb_array_append(proc->blocks, block);
	return block;
}


void ssa_begin_procedure_body(ssaProcedure *proc) {
	gb_array_init(proc->blocks, gb_heap_allocator());
	proc->curr_block = ssa_add_block(proc, proc->type_expr, make_string("entry"));

	if (proc->type->procedure.params != NULL) {
		auto *params = &proc->type->procedure.params->tuple;
		for (isize i = 0; i < params->variable_count; i++) {
			Entity *e = params->variables[i];
			ssa_add_param(proc, e);
		}
	}
}

void ssa_end_procedure_body(ssaProcedure *proc) {
	if (proc->type->procedure.result_count == 0) {
		ssa_emit_ret(proc, NULL);
	}

// Number blocks and registers
	i32 reg_id = 0;
	gb_for_array(i, proc->blocks) {
		ssaBlock *b = proc->blocks[i];
		b->id = i;
		gb_for_array(j, b->instrs) {
			ssaValue *value = b->instrs[j];
			GB_ASSERT(value->kind == ssaValue_Instr);
			ssaInstr *instr = &value->instr;
			// NOTE(bill): Ignore non-returning instructions
			switch (instr->kind) {
			case ssaInstr_Store:
			case ssaInstr_Br:
			case ssaInstr_Ret:
			case ssaInstr_Unreachable:
				continue;
			}
			value->id = reg_id;
			reg_id++;
		}
	}
}

void ssa_push_target_list(ssaProcedure *proc, ssaBlock *break_, ssaBlock *continue_, ssaBlock *fallthrough_) {
	ssaTargetList *tl = gb_alloc_item(proc->module->allocator, ssaTargetList);
	tl->prev          = proc->target_list;
	tl->break_        = break_;
	tl->continue_     = continue_;
	tl->fallthrough_  = fallthrough_;
	proc->target_list = tl;
}

void ssa_pop_target_list(ssaProcedure *proc) {
	proc->target_list = proc->target_list->prev;
}




ssaValue *ssa_emit_conv(ssaProcedure *proc, ssaValue *value, Type *t) {
	Type *src_type = ssa_value_type(value);
	if (are_types_identical(t, src_type))
		return value;

	Type *dst = get_base_type(t);
	Type *src = get_base_type(src_type);

	if (value->kind == ssaValue_Constant) {
		if (dst->kind == Type_Basic)
			return ssa_make_value_constant(proc->module->allocator, t, value->constant.value);
	}


	GB_PANIC("TODO(bill): ssa_emit_conv");

	return NULL;
}

ssaValue *ssa_emit_arith(ssaProcedure *proc, Token op, ssaValue *left, ssaValue *right, Type *type) {
	switch (op.kind) {
	case Token_AndNot: {
		// NOTE(bill): x &~ y == x & (~y) == x & (y ~ -1)
		// NOTE(bill): "not" `x` == `x` "xor" `-1`
		ssaValue *neg = ssa_make_value_constant(proc->module->allocator, type, make_exact_value_integer(-1));
		op.kind = Token_Xor;
		right = ssa_emit_arith(proc, op, right, neg, type);
		ssa_value_set_type(right, type);
		op.kind = Token_And;
	} /* fallthrough */
	case Token_Add:
	case Token_Sub:
	case Token_Mul:
	case Token_Quo:
	case Token_Mod:
	case Token_And:
	case Token_Or:
	case Token_Xor:
		left  = ssa_emit_conv(proc, left, type);
		right = ssa_emit_conv(proc, right, type);
		break;
	}

	ssaValue *v = ssa_make_instr_binary_op(proc, op, left, right);
	return ssa_emit(proc, v);
}

ssaValue *ssa_emit_comp(ssaProcedure *proc, Token op, ssaValue *left, ssaValue *right) {
	Type *a = get_base_type(ssa_value_type(left));
	Type *b = get_base_type(ssa_value_type(right));

	if (are_types_identical(a, b)) {
		// NOTE(bill): No need for a conversion
	} else if (left->kind == ssaValue_Constant) {
		left = ssa_emit_conv(proc, left, ssa_value_type(right));
	} else if (right->kind == ssaValue_Constant) {
		right = ssa_emit_conv(proc, right, ssa_value_type(left));
	}

	ssaValue *v = ssa_make_instr_binary_op(proc, op, left, right);
	ssa_value_set_type(v, &basic_types[Basic_bool]);
	return ssa_emit(proc, v);
}

ssaValue *ssa_emit_ptr_offset(ssaProcedure *proc, ssaValue *ptr, ssaValue *offset) {
	Type *type = ssa_value_type(ptr);
	ssaValue *gep = NULL;
	offset = ssa_emit_conv(proc, offset, t_int);
	gep = ssa_make_instr_get_element_ptr(proc, ptr, offset, NULL, 1, false);
	gep->instr.get_element_ptr.element_type = type_deref(type);
	gep->instr.get_element_ptr.result_type  = type;
	return ssa_emit(proc, gep);
}

ssaValue *ssa_emit_struct_gep(ssaProcedure *proc, ssaValue *s, ssaValue *index, Type *result_type) {
	ssaValue *gep = NULL;
	// NOTE(bill): For some weird legacy reason in LLVM, structure elements must be accessed as an i32
	index = ssa_emit_conv(proc, index, t_i32);
	gep = ssa_make_instr_get_element_ptr(proc, s, v_zero, index, 2, true);
	gep->instr.get_element_ptr.element_type = ssa_value_type(s);
	gep->instr.get_element_ptr.result_type = result_type;

	return ssa_emit(proc, gep);
}


ssaValue *ssa_array_elem(ssaProcedure *proc, ssaValue *array) {
	Type *t = ssa_value_type(array);
	GB_ASSERT(t->kind == Type_Array);
	Type *base_type = t->array.element;
	ssaValue *elem = ssa_make_instr_get_element_ptr(proc, array, v_zero, v_zero, 2, true);
	Type *result_type = make_type_pointer(proc->module->allocator, base_type);
	elem->instr.get_element_ptr.element_type = t;
	elem->instr.get_element_ptr.result_type = result_type;
	return ssa_emit(proc, elem);
}
ssaValue *ssa_array_len(ssaProcedure *proc, ssaValue *array) {
	Type *t = ssa_value_type(array);
	GB_ASSERT(t->kind == Type_Array);
	return ssa_make_value_constant(proc->module->allocator, t_int, make_exact_value_integer(t->array.count));
}
ssaValue *ssa_array_cap(ssaProcedure *proc, ssaValue *array) {
	return ssa_array_len(proc, array);
}

ssaValue *ssa_slice_elem(ssaProcedure *proc, ssaValue *slice) {
	Type *t = ssa_value_type(slice);
	GB_ASSERT(t->kind == Type_Slice);

	Type *result_type = make_type_pointer(proc->module->allocator, t->slice.element);
	return ssa_emit_load(proc, ssa_emit_struct_gep(proc, slice, v_zero32, result_type));
}
ssaValue *ssa_slice_len(ssaProcedure *proc, ssaValue *slice) {
	Type *t = ssa_value_type(slice);
	GB_ASSERT(t->kind == Type_Slice);
	return ssa_emit_load(proc, ssa_emit_struct_gep(proc, slice, v_one32, t_int));
}
ssaValue *ssa_slice_cap(ssaProcedure *proc, ssaValue *slice) {
	Type *t = ssa_value_type(slice);
	GB_ASSERT(t->kind == Type_Slice);
	return ssa_emit_load(proc, ssa_emit_struct_gep(proc, slice, v_two32, t_int));
}





ssaValue *ssa_emit_slice(ssaProcedure *proc, Type *slice_type, ssaValue *base, ssaValue *low, ssaValue *high, ssaValue *max) {
	// TODO(bill): array bounds checking for slice creation
	// TODO(bill): check that low < high <= max
	gbAllocator a = proc->module->allocator;
	Type *base_type = get_base_type(ssa_value_type(base));

	if (low == NULL) {
		low = v_zero;
	}
	if (high == NULL) {
		switch (base_type->kind) {
		case Type_Array:   high = ssa_array_len(proc, base); break;
		case Type_Slice:   high = ssa_slice_len(proc, base); break;
		case Type_Pointer: high = v_one;                     break;
		}
	}
	if (max == NULL) {
		switch (base_type->kind) {
		case Type_Array:   max = ssa_array_cap(proc, base); break;
		case Type_Slice:   max = ssa_slice_cap(proc, base); break;
		case Type_Pointer: max = high;                      break;
		}
	}
	GB_ASSERT(max != NULL);

	Token op_sub = {Token_Sub};
	ssaValue *len = ssa_emit_arith(proc, op_sub, high, low, t_int);
	ssaValue *cap = ssa_emit_arith(proc, op_sub, max,  low, t_int);

	ssaValue *elem = NULL;
	switch (base_type->kind) {
	case Type_Array:   elem = ssa_array_elem(proc, base); break;
	case Type_Slice:   elem = ssa_slice_elem(proc, base); break;
	case Type_Pointer: elem = base;                       break;
	}

	elem = ssa_emit_ptr_offset(proc, elem, low);

	// NOTE(bill): Just used as dummy entity - never to be used really
	Entity *slice_entity = make_entity_variable(proc->module->allocator,
	                                            proc->curr_block->scope,
	                                            empty_token,
	                                            slice_type);

	ssaValue *slice = ssa_emit(proc, ssa_make_instr_local(proc, slice_entity));

	ssaValue *gep = NULL;
	gep = ssa_emit_struct_gep(proc, slice, v_zero32, ssa_value_type(elem));
	ssa_emit_store(proc, gep, elem);

	gep = ssa_emit_struct_gep(proc, slice, v_one32, t_int);
	ssa_emit_store(proc, gep, len);

	gep = ssa_emit_struct_gep(proc, slice, v_two32, t_int);
	ssa_emit_store(proc, gep, cap);

	return ssa_emit_load(proc, slice);
}


ssaValue *ssa_build_single_expr(ssaProcedure *proc, AstNode *expr, TypeAndValue *tv) {
	switch (expr->kind) {
	case_ast_node(bl, BasicLit, expr);
		GB_PANIC("Non-constant basic literal");
	case_end;

	case_ast_node(i, Ident, expr);
		Entity *e = *map_get(&proc->module->info->uses, hash_pointer(expr));
		if (e->kind == Entity_Builtin) {
			GB_PANIC("TODO(bill): Entity_Builtin");
			return NULL;
		}

		auto *found = map_get(&proc->module->values, hash_pointer(e));
		if (found) {
			ssaValue *v = *found;
			if (v->kind == ssaValue_Proc)
				return v;
			return ssa_emit_load(proc, v);
		}
	case_end;

	case_ast_node(pe, ParenExpr, expr);
		return ssa_build_single_expr(proc, unparen_expr(expr), tv);
	case_end;

	case_ast_node(de, DerefExpr, expr);
		return ssa_lvalue_load(ssa_build_addr(proc, expr), proc);
	case_end;

	case_ast_node(se, SelectorExpr, expr);
		return ssa_lvalue_load(ssa_build_addr(proc, expr), proc);
	case_end;

	case_ast_node(ue, UnaryExpr, expr);
		switch (ue->op.kind) {
		case Token_Pointer:
			return ssa_lvalue_address(ssa_build_addr(proc, ue->expr), proc);
		case Token_Add:
			return ssa_build_expr(proc, ue->expr);
		case Token_Sub: {
			// NOTE(bill): -`x` == 0 - `x`
			ssaValue *left = v_zero;
			ssaValue *right = ssa_build_expr(proc, ue->expr);
			return ssa_emit_arith(proc, ue->op, left, right, tv->type);
		} break;
		case Token_Xor: { // Bitwise not
			// NOTE(bill): "not" `x` == `x` "xor" `-1`
			ExactValue neg_one = make_exact_value_integer(-1);
			ssaValue *left = ssa_build_expr(proc, ue->expr);
			ssaValue *right = ssa_make_value_constant(proc->module->allocator, tv->type, neg_one);
			return ssa_emit_arith(proc, ue->op, left, right, tv->type);
		} break;
		case Token_Not: // Boolean not
			GB_PANIC("Token_Not");
			return NULL;

		}
	case_end;

	case_ast_node(be, BinaryExpr, expr);
		switch (be->op.kind) {
		case Token_Add:
		case Token_Sub:
		case Token_Mul:
		case Token_Quo:
		case Token_Mod:
		case Token_And:
		case Token_Or:
		case Token_Xor:
		case Token_AndNot:
			return ssa_emit_arith(proc, be->op,
			                      ssa_build_expr(proc, be->left),
			                      ssa_build_expr(proc, be->right),
			                      tv->type);


		case Token_CmpEq:
		case Token_NotEq:
		case Token_Lt:
		case Token_LtEq:
		case Token_Gt:
		case Token_GtEq: {
			ssaValue *left  = ssa_build_expr(proc, be->left);
			ssaValue *right = ssa_build_expr(proc, be->right);
			ssaValue *cmp = ssa_emit_comp(proc, be->op, left, right);
			return ssa_emit_conv(proc, cmp, default_type(tv->type));
		} break;

		default:
			GB_PANIC("Invalid binary expression");
			break;
		}
	case_end;

	case_ast_node(se, ProcLit, expr);
		GB_PANIC("TODO(bill): ssa_build_single_expr ProcLit");
	case_end;

	case_ast_node(se, CastExpr, expr);
		GB_PANIC("TODO(bill): ssa_build_single_expr CastExpr");
	case_end;

	case_ast_node(se, CallExpr, expr);
		GB_PANIC("TODO(bill): ssa_build_single_expr CallExpr");
	case_end;

	case_ast_node(se, SliceExpr, expr);
		ssaValue *base = NULL;
		ssaValue *low  = NULL;
		ssaValue *high = NULL;
		ssaValue *max  = NULL;
		switch (tv->type->kind) {
		case Type_Slice:
		case Type_Array:
			base = ssa_lvalue_address(ssa_build_addr(proc, se->expr), proc);
			break;
		case Type_Basic:
			GB_PANIC("SliceExpr Type_Basic");
			break;
		}

		if (se->low  != NULL)    low  = ssa_build_expr(proc, se->low);
		if (se->high != NULL)    high = ssa_build_expr(proc, se->high);
		if (se->triple_indexed)  max  = ssa_build_expr(proc, se->max);

		return ssa_emit_slice(proc, tv->type, base, low, high, max);
	case_end;

	case_ast_node(ie, IndexExpr, expr);
		Type *t = type_of_expr(proc->module->info, ie->expr);
		t = get_base_type(t);
		switch (t->kind) {
		case Type_Basic: {
			// TODO(bill): Strings AstNode_IndexExpression
		} break;

		case Type_Slice:
		case Type_Array:
		case Type_Pointer: {
			ssaValue *v = ssa_lvalue_address(ssa_build_addr(proc, expr), proc);
			return ssa_emit_load(proc, v);
		} break;
		}
	case_end;
	}

	GB_PANIC("Unexpected expression");
	return NULL;
}


ssaValue *ssa_build_expr(ssaProcedure *proc, AstNode *expr) {
	expr = unparen_expr(expr);

	TypeAndValue *tv = map_get(&proc->module->info->types, hash_pointer(expr));
	if (tv) {
		if (tv->value.kind != ExactValue_Invalid) {
			if (tv->value.kind == ExactValue_String) {
				ssaValue *global_str = ssa_add_global_string(proc, tv->value);
				return ssa_emit_load(proc, global_str);
			}
			return ssa_make_value_constant(proc->module->allocator, tv->type, tv->value);
		}

		ssaValue *value = NULL;
		if (tv->mode == Addressing_Variable) {
			value = ssa_lvalue_load(ssa_build_addr(proc, expr), proc);
		} else {
			value = ssa_build_single_expr(proc, expr, tv);
		}

		return value;
	}
	return NULL;
}


ssaLvalue ssa_build_addr(ssaProcedure *proc, AstNode *expr) {
	switch (expr->kind) {
	case_ast_node(i, Ident, expr);
		if (ssa_is_blank_ident(expr)) {
			ssaLvalue val = {ssaLvalue_Blank};
			return val;
		}

		Entity *e = entity_of_ident(proc->module->info, expr);
		ssaValue *v = NULL;
		ssaValue **found = map_get(&proc->module->values, hash_pointer(e));
		if (found) v = *found;
		return ssa_make_lvalue_address(v, expr);
	case_end;

	case_ast_node(pe, ParenExpr, expr);
		return ssa_build_addr(proc, unparen_expr(expr));
	case_end;

	case_ast_node(se, SelectorExpr, expr);
		AstNode *selector = unparen_expr(se->selector);
		Type *type = type_of_expr(proc->module->info, se->expr);

		isize index = 0;
		Entity *entity = lookup_field(type, selector, &index);
		GB_ASSERT(entity != NULL);

		ssaValue *v = ssa_lvalue_address(ssa_build_addr(proc, se->expr), proc);

		if (type->kind == Type_Pointer) {
			// NOTE(bill): Allow x^.y and x.y to be the same
			type = type_deref(type);
			v = ssa_emit_load(proc, v);
		}

		ssaValue *i0 = v_zero32;
		ssaValue *i1 = ssa_make_value_constant(proc->module->allocator, t_i32, make_exact_value_integer(index));
		ssaValue *gep = ssa_make_instr_get_element_ptr(proc, v, i0, i1, 2, true);
		gep->instr.get_element_ptr.result_type = entity->type;
		gep->instr.get_element_ptr.element_type = type;
		v = ssa_emit(proc, gep);
		return ssa_make_lvalue_address(v, expr);
	case_end;

	case_ast_node(ie, IndexExpr, expr);
		ssaValue *v = NULL;
		Type *t = get_base_type(type_of_expr(proc->module->info, ie->expr));
		switch (t->kind) {
		case Type_Array: {
			ssaValue *array = ssa_lvalue_address(ssa_build_addr(proc, ie->expr), proc);
			ssaValue *index = ssa_emit_conv(proc, ssa_build_expr(proc, ie->index), t_int);
			ssaValue *elem = ssa_array_elem(proc, array);
			v = ssa_emit_ptr_offset(proc, elem, index);
		} break;
		case Type_Pointer: {
			ssaValue *ptr = ssa_emit_load(proc, ssa_lvalue_address(ssa_build_addr(proc, ie->expr), proc));
			ssaValue *index = ssa_emit_conv(proc, ssa_build_expr(proc, ie->index), t_int);
			v = ssa_emit_ptr_offset(proc, ptr, index);
		} break;
		case Type_Slice: {
			ssaValue *slice = ssa_lvalue_address(ssa_build_addr(proc, ie->expr), proc);
			ssaValue *index = ssa_emit_conv(proc, ssa_build_expr(proc, ie->index), t_int);
			ssaValue *elem = ssa_slice_elem(proc, slice);
			v = ssa_emit_ptr_offset(proc, elem, index);
		} break;
		}

		// NOTE(bill): lvalue address encodes the pointer, thus the deref
		ssa_value_set_type(v, type_deref(ssa_value_type(v)));
		return ssa_make_lvalue_address(v, expr);
	case_end;

	case_ast_node(de, DerefExpr, expr);
		ssaValue *load = ssa_emit_load(proc, ssa_lvalue_address(ssa_build_addr(proc, de->expr), proc));
		ssaValue *gep = ssa_make_instr_get_element_ptr(proc, load, NULL, NULL, 0, false);
		Type *t = ssa_value_type(load);
		t = type_deref(get_base_type(t));
		gep->instr.get_element_ptr.result_type  = t;
		gep->instr.get_element_ptr.element_type = t;
		return ssa_make_lvalue_address(ssa_emit(proc, gep), expr);
	case_end;
	}

	GB_PANIC("Unexpected address expression");

	ssaLvalue blank = {ssaLvalue_Blank};
	return blank;
}

void ssa_build_assign_op(ssaProcedure *proc, ssaLvalue lhs, ssaValue *value, Token op) {
	ssaValue *old_value = ssa_lvalue_load(lhs, proc);
	ssaValue *change = ssa_emit_conv(proc, value, ssa_value_type(old_value));
	ssaValue *new_value = ssa_emit_arith(proc, op, old_value, change, ssa_lvalue_type(lhs));
	ssa_lvalue_store(lhs, proc, new_value);
}

void ssa_build_cond(ssaProcedure *proc, AstNode *cond, ssaBlock *true_block, ssaBlock *false_block) {
	switch (cond->kind) {
	case_ast_node(pe, ParenExpr, cond);
		ssa_build_cond(proc, pe->expr, true_block, false_block);
		return;
	case_end;

	case_ast_node(ue, UnaryExpr, cond);
		if (ue->op.kind == Token_Not) {
			ssa_build_cond(proc, ue->expr, false_block, true_block);
			return;
		}
	case_end;

	case_ast_node(be, BinaryExpr, cond);
		if (be->op.kind == Token_CmpAnd) {
			ssaBlock *block = ssa_add_block(proc, NULL, make_string("logical-true"));
			ssa_build_cond(proc, be->left, block, false_block);
			proc->curr_block = block;
			ssa_build_cond(proc, be->right, true_block, false_block);
			return;
		} else if (be->op.kind == Token_CmpOr) {
			ssaBlock *block = ssa_add_block(proc, NULL, make_string("logical-false"));
			ssa_build_cond(proc, be->left, true_block, block);
			proc->curr_block = block;
			ssa_build_cond(proc, be->right, true_block, false_block);
			return;
		}
	case_end;
	}

	ssaValue *expr = ssa_build_expr(proc, cond);
	ssa_emit_if(proc, expr, true_block, false_block);
}



void ssa_build_stmt_list(ssaProcedure *proc, AstNode *list) {
	for (AstNode *stmt = list ; stmt != NULL; stmt = stmt->next)
		ssa_build_stmt(proc, stmt);
}

void ssa_build_stmt(ssaProcedure *proc, AstNode *s) {
	switch (s->kind) {
	case_ast_node(bs, EmptyStmt, s);
	case_end;

	case_ast_node(vd, VarDecl, s);
		if (vd->kind == Declaration_Mutable) {
			if (vd->name_count == vd->value_count) { // 1:1 assigment
				gbArray(ssaLvalue)  lvals;
				gbArray(ssaValue *) inits;
				gb_array_init_reserve(lvals, gb_heap_allocator(), vd->name_count);
				gb_array_init_reserve(inits, gb_heap_allocator(), vd->name_count);
				defer (gb_array_free(lvals));
				defer (gb_array_free(inits));

				for (AstNode *name = vd->name_list; name != NULL; name = name->next) {
					ssaLvalue lval = {ssaLvalue_Blank};
					if (!ssa_is_blank_ident(name)) {
						ssa_add_local_for_identifier(proc, name);
						lval = ssa_build_addr(proc, name);
					}

					gb_array_append(lvals, lval);
				}

				for (AstNode *value = vd->value_list; value != NULL; value = value->next) {
					ssaValue *init = ssa_build_expr(proc, value);
					gb_array_append(inits, init);
				}


				gb_for_array(i, inits) {
					ssa_lvalue_store(lvals[i], proc, inits[i]);
				}

			} else if (vd->value_count == 0) { // declared and zero-initialized
				for (AstNode *name = vd->name_list; name != NULL; name = name->next) {
					if (!ssa_is_blank_ident(name)) {
						// TODO(bill): add local
						ssa_add_local_for_identifier(proc, name);
					}
				}
			} else { // Tuple(s)
				GB_PANIC("TODO(bill): tuple assignment variable declaration");
			}
		}
	case_end;

	case_ast_node(ids, IncDecStmt, s);
		Token op = ids->op;
		if (op.kind == Token_Increment) {
			op.kind = Token_Add;
		} else if (op.kind == Token_Decrement) {
			op.kind = Token_Sub;
		}
		ssaLvalue lval = ssa_build_addr(proc, ids->expr);
		ssaValue *one = ssa_emit_conv(proc, v_one, ssa_lvalue_type(lval));
		ssa_build_assign_op(proc, lval, one, op);

	case_end;

	case_ast_node(as, AssignStmt, s);
		switch (as->op.kind) {
		case Token_Eq: {
			gbArray(ssaLvalue) lvals;
			gb_array_init(lvals, gb_heap_allocator());
			defer (gb_array_free(lvals));

			for (AstNode *lhs = as->lhs_list;
			     lhs != NULL;
			     lhs = lhs->next) {
				ssaLvalue lval = {};
				if (!ssa_is_blank_ident(lhs)) {
					lval = ssa_build_addr(proc, lhs);
				}
				gb_array_append(lvals, lval);
			}

			if (as->lhs_count == as->rhs_count) {
				if (as->lhs_count == 1) {
					AstNode *lhs = as->lhs_list;
					AstNode *rhs = as->rhs_list;
					ssaValue *init = ssa_build_expr(proc, rhs);
					ssa_lvalue_store(lvals[0], proc, init);
				} else {
					gbArray(ssaValue *) inits;
					gb_array_init_reserve(inits, gb_heap_allocator(), gb_array_count(lvals));
					defer (gb_array_free(inits));

					for (AstNode *rhs = as->rhs_list; rhs != NULL; rhs = rhs->next) {
						ssaValue *init = ssa_build_expr(proc, rhs);
						gb_array_append(inits, init);
					}

					gb_for_array(i, inits) {
						ssa_lvalue_store(lvals[i], proc, inits[i]);
					}
				}
			} else {
				GB_PANIC("TODO(bill): tuple assignment");
			}

		} break;

		default: {
			// NOTE(bill): Only 1 += 1 is allowed, no tuples
			// +=, -=, etc
			Token op = as->op;
			i32 kind = op.kind;
			kind += Token_Add - Token_AddEq; // Convert += to +
			op.kind = cast(TokenKind)kind;
			ssaLvalue lhs = ssa_build_addr(proc, as->lhs_list);
			ssaValue *value = ssa_build_expr(proc, as->rhs_list);
			ssa_build_assign_op(proc, lhs, value, op);
		} break;
		}
	case_end;

	case_ast_node(es, ExprStmt, s);
		ssa_build_expr(proc, es->expr);
	case_end;

	case_ast_node(bs, BlockStmt, s)
		ssa_build_stmt_list(proc, bs->list);
	case_end;

	case_ast_node(bs, DeferStmt, s);
		// NOTE(bill): is already handled with scope
	case_end;

	case_ast_node(rs, ReturnStmt, s);
		ssaValue *v = NULL;
		auto *return_type_tuple  = &proc->type->procedure.results->tuple;
		isize return_count = proc->type->procedure.result_count;
		if (rs->result_count == 1 && return_count > 1) {
			GB_PANIC("ReturnStmt tuple return statement");
		} else if (return_count == 1) {
			Entity *e = return_type_tuple->variables[0];
			v = ssa_emit_conv(proc, ssa_build_expr(proc, rs->result_list), e->type);
		} else if (return_count == 0) {
			// No return values
		} else {
			// 1:1 multiple return values
		}

		ssa_emit_ret(proc, v);

	case_end;

	case_ast_node(is, IfStmt, s);
		if (is->init != NULL) {
			ssa_build_stmt(proc, is->init);
		}
		ssaBlock *then = ssa_add_block(proc, s, make_string("if.then"));
		ssaBlock *done = ssa__make_block(proc, s, make_string("if.done"));
		ssaBlock *else_ = done;
		if (is->else_stmt != NULL) {
			else_ = ssa_add_block(proc, is->else_stmt, make_string("if.else"));
		}

		ssa_build_cond(proc, is->cond, then, else_);
		proc->curr_block = then;
		ssa_build_stmt(proc, is->body);
		ssa_emit_jump(proc, done);

		if (is->else_stmt != NULL) {
			proc->curr_block = else_;
			ssa_build_stmt(proc, is->else_stmt);
			ssa_emit_jump(proc, done);
		}
		gb_array_append(proc->blocks, done);
		proc->curr_block = done;
	case_end;

	case_ast_node(fs, ForStmt, s);
		if (fs->init != NULL) {
			ssa_build_stmt(proc, fs->init);
		}
		ssaBlock *body = ssa_add_block(proc, s, make_string("for.body"));
		ssaBlock *done = ssa__make_block(proc, s, make_string("for.done")); // NOTE(bill): Append later

		ssaBlock *loop = body;

		if (fs->cond != NULL) {
			loop = ssa_add_block(proc, fs->cond, make_string("for.loop"));
		}
		ssaBlock *cont = loop;
		if (fs->post != NULL) {
			cont = ssa_add_block(proc, fs->cond, make_string("for.post"));
		}
		ssa_emit_jump(proc, loop);
		proc->curr_block = loop;
		if (loop != body) {
			ssa_build_cond(proc, fs->cond, body, done);
			proc->curr_block = body;
		}

		ssa_push_target_list(proc, done, cont, NULL);
		ssa_build_stmt(proc, fs->body);
		ssa_pop_target_list(proc);
		ssa_emit_jump(proc, cont);

		if (fs->post != NULL) {
			proc->curr_block = cont;
			ssa_build_stmt(proc, fs->post);
			ssa_emit_jump(proc, loop);
		}
		gb_array_append(proc->blocks, done);
		proc->curr_block = done;

	case_end;

	case_ast_node(bs, BranchStmt, s);
		ssaBlock *block = NULL;
		switch (bs->token.kind) {
		case Token_break: {
			for (ssaTargetList *t = proc->target_list;
			     t != NULL && block == NULL;
			     t = t->prev) {
				block = t->break_;
			}
		} break;
		case Token_continue: {
			for (ssaTargetList *t = proc->target_list;
			     t != NULL && block == NULL;
			     t = t->prev) {
				block = t->continue_;
			}
		} break;
		case Token_fallthrough: {
			for (ssaTargetList *t = proc->target_list;
			     t != NULL && block == NULL;
			     t = t->prev) {
				block = t->fallthrough_;
			}
		} break;
		}
		ssa_emit_jump(proc, block);
		ssa_emit_unreachable(proc);
	case_end;

	}
}

void ssa_build_proc(ssaValue *value) {
	ssaProcedure *proc = &value->proc;

	AstNode *proc_decl = proc->decl->proc_decl;
	switch (proc_decl->kind) {
	case_ast_node(pd, ProcDecl, proc_decl);
		proc->type_expr = pd->type;
		proc->body = pd->body;
	case_end;
	default:
		return;
	}

	if (proc->body != NULL) {
		ssa_begin_procedure_body(proc);
		ssa_build_stmt(proc, proc->body);
		ssa_end_procedure_body(proc);
	}
}










