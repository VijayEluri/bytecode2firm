#ifndef GCJ_INTERFACE_H
#define GCJ_INTERFACE_H

#include <libfirm/firm.h>
#include "mangle.h"

#define GCJI_LENGTH_OFFSET 4
#define GCJI_DATA_OFFSET   8

#define GCJI_VTABLE_OFFSET 2

void       gcji_init(void);
int        gcji_is_api_class(ir_type *type);
void       gcji_class_init(ir_type *type, ir_graph *irg, ir_node *block, ir_node **mem);
ir_node   *gcji_allocate_object(ir_type *type, ir_graph *irg, ir_node *block, ir_node **mem);
ir_node   *gcji_allocate_array(ir_type *eltype, ir_node *count, ir_graph *irg, ir_node *block, ir_node **mem);
ir_node   *gcji_new_string(ir_entity *bytes, ir_graph *irg, ir_node *block, ir_node **mem);
ir_node   *gcji_get_arraylength(ir_node *arrayref, ir_graph *irg, ir_node *block, ir_node **mem);
ir_entity *gcji_get_class_dollar_field(ir_type *classtype);
ir_node   *gcji_lookup_interface(ir_node *obptr, ir_type *iface, ir_entity *method, ir_graph *irg, ir_node *block, ir_node **mem);

#endif
