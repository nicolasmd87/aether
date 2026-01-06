#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>

// Aether runtime libraries
#include "actor_state_machine.h"
#include "multicore_scheduler.h"
#include "aether_string.h"
#include "aether_io.h"
#include "aether_math.h"
#include "aether_supervision.h"
#include "aether_tracing.h"
#include "aether_bounds_check.h"
#include "aether_runtime_types.h"

extern __thread int current_core_id;

void add(void a, void b) {
    {
return (a + b);
    }
}

int safe_divide(void x, void y) {
    {
AetherString* t1 = aether_typeof(x);
AetherString* t2 = aether_typeof(y);
if ((y == 0)) {
            {
printf("%s\n", aether_string_from_literal("Error: Division by zero")->data);
return 0;
            }
        }
return (x / y);
    }
}

AetherString* format_value(void v, AetherString* label) {
    {
AetherString* type_name = aether_typeof(v);
return ((label + aether_string_from_literal(": ")) + type_name);
    }
}

int main() {
    {
printf("%s\n", aether_string_from_literal("=== Dynamic Typing Demo ===")->data);
int x = 42;
float y = 3.14;
AetherString* z = aether_string_from_literal("hello");
printf("%s\n", (aether_string_from_literal("x type: ") + aether_typeof(x))->data);
printf("%s\n", (aether_string_from_literal("y type: ") + aether_typeof(y))->data);
printf("%s\n", (aether_string_from_literal("z type: ") + aether_typeof(z))->data);
void result1 = add(10, 20);
void result2 = add(5, 7);
printf("%s\n", (aether_string_from_literal("add(10, 20) = ") + result1)->data);
printf("%s\n", (aether_string_from_literal("add(5, 7) = ") + result2)->data);
int r1 = safe_divide(10, 2);
int r2 = safe_divide(10, 0);
printf("%s\n", (aether_string_from_literal("safe_divide(10, 2) = ") + r1)->data);
printf("%s\n", (aether_string_from_literal("safe_divide(10, 0) = ") + r2)->data);
AetherString* msg1 = format_value(100, aether_string_from_literal("Count"));
AetherString* msg2 = format_value(3.14, aether_string_from_literal("Pi"));
printf("%s\n", msg1->data);
printf("%s\n", msg2->data);
printf("%s\n", aether_string_from_literal("=== Demo Complete ===")->data);
    }
    return 0;
}
