//
// Created by onur on 07.10.2018.
//

#ifndef ZEROSCRIPT_GARBAGE_COLLECTOR_H
#define ZEROSCRIPT_GARBAGE_COLLECTOR_H

#include "object.h"

void gc_visit_state(z_interpreter_state_t *state);

void gc_visit_object(z_object_t *object);

void gc_visit_object_with_map(z_object_t *object);

void gc_visit_context(const z_object_t *context);

void gc_visit_instance(z_object_t *object);

void gc_visit_function_ref(z_object_t *object);

void gc_free_object(z_object_t *object);

void gc_visit_stack(z_reg_t *stack_start, z_reg_t *stack_ptr);

void gc_visit_register(z_reg_t *reg);

uhalf_int_t gc_version = 0;

int_t gc() {
    gc_version++;
    // iterate all the objects
    for (int_t i = 0; i < interpreter_states_list->size; i++) {
        z_interpreter_state_t *state = *(z_interpreter_state_t **) arraylist_get(interpreter_states_list, i);
        // if it is a removed gc root, skip it
        if (!state->removed_as_a_root) {
            gc_visit_state(state);
        }
    }
    // iterate static variables
    arraylist_t* known_types = object_manager_get_known_classes();
    for(int i = 0; i < known_types->size;i++){
        z_type_info_t* type = (z_type_info_t*)arraylist_get(known_types,i);
        map_t* static_variable_map = type->static_variables;
        arraylist_t* static_variable_names = map_key_list(static_variable_map);
        for(int j = 0;j<static_variable_names->size;j++){
            gc_visit_register((z_reg_t*)map_get(static_variable_map,*(char**)arraylist_get(static_variable_names,j)));
        }
    }

    arraylist_t *new_list = arraylist_new_capacity(sizeof(int_t),gc_objects_list->size );
    for (int_t i = 0; i < gc_objects_list->size; i++) {
        z_object_t *object = *(z_object_t **) arraylist_get(gc_objects_list, i);
        if (object->gc_version != gc_version) {
            gc_free_object(object);
        } else {
            arraylist_push(new_list, &object);
        }
    }
    z_free(gc_objects_list->data);
    z_free(gc_objects_list);
    gc_objects_list = new_list;
}

void gc_free_object(z_object_t *object) {
    switch (object->type) {
        case TYPE_STR:
            z_free(object->string_object.value);
            break;
        case TYPE_CONTEXT:
            if (object->context_object.locals)
                z_free(object->context_object.locals);
            if (object->context_object.catches_list)
                arraylist_free(object->context_object.catches_list);
            if (object->context_object.symbol_table)
                map_free(object->context_object.symbol_table);
            break;
        case TYPE_FUNCTION_REF:
            break;
        case TYPE_CLASS_REF:
            break;
        case TYPE_INSTANCE:
            z_free(object->instance_object.saved_state);
            break;
        case TYPE_OBJ:
            map_free(object->properties);
            if (object->key_list_cache) {
                z_free(object->key_list_cache);
            }
            break;
    }
    z_free(object);
}

void gc_visit_state(z_interpreter_state_t *state) {
    z_object_t *root_context = (z_object_t *) (state->root_context);
    z_object_t *context = (z_object_t *) (state->current_context);
    gc_visit_object(context);
    gc_visit_object(root_context);
    gc_visit_stack(state->stack_start, state->stack_ptr);
}

void gc_visit_stack(z_reg_t *stack_start, z_reg_t *stack_ptr) {
    for (int_t i = 0; i < stack_ptr - stack_start; i++) {
        z_reg_t reg = stack_start[i + 1];
        gc_visit_register(&reg);
    }
}

void gc_visit_register(z_reg_t *reg) {
    if (reg->type != TYPE_NUMBER && reg->type != TYPE_NATIVE_FUNC) {
        z_object_t *value = (z_object_t *) reg->val;
        gc_visit_object(value);
    }
}

void gc_visit_context(const z_object_t *context) {
    z_reg_t *locals = (z_reg_t *) (context->context_object.locals);
    uint_t locals_count = context->context_object.locals_count;
    for (int_t i = 2; i < locals_count; i++) {
        z_reg_t reg = locals[i];
        gc_visit_register(&reg);
    }
    gc_visit_object((z_object_t *) (context->context_object.parent_context));
    gc_visit_object((z_object_t *) (context->context_object.return_context));
}

void gc_visit_object(z_object_t *object) {
    if (!object) return;
    if (object->gc_version == gc_version) return;
    object->gc_version = gc_version;
    switch (object->type) {
        case TYPE_OBJ:
            gc_visit_object_with_map(object);
            break;
        case TYPE_CONTEXT:
            gc_visit_context(object);
            break;
        case TYPE_INSTANCE:
            gc_visit_instance(object);
            break;
        case TYPE_FUNCTION_REF:
            gc_visit_function_ref(object);
            break;
    }
}

void gc_visit_function_ref(z_object_t *object) {
    gc_visit_object((z_object_t *) (object->function_ref_object.parent_context));
    gc_visit_state(object->function_ref_object.responsible_interpreter_state);
}

void gc_visit_instance(z_object_t *object) {
    gc_visit_state(object->instance_object.saved_state);
}

void gc_visit_object_with_map(z_object_t *object) {
    map_t *self = object->properties;
    for (int i = 0; i < MAP_BAG_SIZE; i++) {
        arraylist_t *kbag = self->keys[i];
        arraylist_t *vbag = self->values[i];
        if (kbag) {
            for (int_t j = 0; j < kbag->size; j++) {
                gc_visit_register((z_reg_t *) arraylist_get(vbag, j));
            }
        }
    }
}

#endif //ZEROSCRIPT_GARBAGE_COLLECTOR_H
