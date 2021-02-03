add_executable(shader_recompiler
    environment.h
    exception.h
    file_environment.cpp
    file_environment.h
    frontend/ir/attribute.cpp
    frontend/ir/attribute.h
    frontend/ir/basic_block.cpp
    frontend/ir/basic_block.h
    frontend/ir/condition.cpp
    frontend/ir/condition.h
    frontend/ir/flow_test.cpp
    frontend/ir/flow_test.h
    frontend/ir/function.cpp
    frontend/ir/function.h
    frontend/ir/ir_emitter.cpp
    frontend/ir/ir_emitter.h
    frontend/ir/microinstruction.cpp
    frontend/ir/microinstruction.h
    frontend/ir/opcode.cpp
    frontend/ir/opcode.h
    frontend/ir/opcode.inc
    frontend/ir/pred.h
    frontend/ir/reg.h
    frontend/ir/type.cpp
    frontend/ir/type.h
    frontend/ir/value.cpp
    frontend/ir/value.h
    frontend/maxwell/control_flow.cpp
    frontend/maxwell/control_flow.h
    frontend/maxwell/decode.cpp
    frontend/maxwell/decode.h
    frontend/maxwell/instruction.h
    frontend/maxwell/location.h
    frontend/maxwell/maxwell.inc
    frontend/maxwell/opcode.cpp
    frontend/maxwell/opcode.h
    frontend/maxwell/program.cpp
    frontend/maxwell/program.h
    frontend/maxwell/termination_code.cpp
    frontend/maxwell/termination_code.h
    frontend/maxwell/translate/impl/floating_point_conversion_integer.cpp
    frontend/maxwell/translate/impl/floating_point_multi_function.cpp
    frontend/maxwell/translate/impl/impl.cpp
    frontend/maxwell/translate/impl/impl.h
    frontend/maxwell/translate/impl/load_store_attribute.cpp
    frontend/maxwell/translate/impl/load_store_memory.cpp
    frontend/maxwell/translate/impl/not_implemented.cpp
    frontend/maxwell/translate/impl/register_move.cpp
    frontend/maxwell/translate/translate.cpp
    frontend/maxwell/translate/translate.h
    ir_opt/dead_code_elimination_pass.cpp
    ir_opt/get_set_elimination_pass.cpp
    ir_opt/identity_removal_pass.cpp
    ir_opt/passes.h
    ir_opt/ssa_rewrite_pass.cpp
    ir_opt/verification_pass.cpp
    main.cpp
)
target_link_libraries(shader_recompiler PRIVATE fmt::fmt)

if (MSVC)
    target_compile_options(shader_recompiler PRIVATE
        /W4
        /WX
        /we4018 # 'expression' : signed/unsigned mismatch
        /we4244 # 'argument' : conversion from 'type1' to 'type2', possible loss of data (floating-point)
        /we4245 # 'conversion' : conversion from 'type1' to 'type2', signed/unsigned mismatch
        /we4254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
        /we4267 # 'var' : conversion from 'size_t' to 'type', possible loss of data
        /we4305 # 'context' : truncation from 'type1' to 'type2'
        /we4800 # Implicit conversion from 'type' to bool. Possible information loss
        /we4826 # Conversion from 'type1' to 'type2' is sign-extended. This may cause unexpected runtime behavior.
    )
else()
    target_compile_options(shader_recompiler PRIVATE
        -Werror
        -Werror=conversion
        -Werror=ignored-qualifiers
        -Werror=implicit-fallthrough
        -Werror=shadow
        -Werror=sign-compare
        $<$<CXX_COMPILER_ID:GNU>:-Werror=unused-but-set-parameter>
        $<$<CXX_COMPILER_ID:GNU>:-Werror=unused-but-set-variable>
        -Werror=unused-variable
    )
endif()

create_target_directory_groups(shader_recompiler)
