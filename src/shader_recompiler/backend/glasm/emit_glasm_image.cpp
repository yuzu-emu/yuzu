// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>

#include "shader_recompiler/backend/glasm/emit_context.h"
#include "shader_recompiler/backend/glasm/emit_glasm_instructions.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::GLASM {
namespace {
struct ScopedRegister {
    ScopedRegister() = default;
    ScopedRegister(RegAlloc& reg_alloc_) : reg_alloc{&reg_alloc_}, reg{reg_alloc->AllocReg()} {}

    ~ScopedRegister() {
        if (reg_alloc) {
            reg_alloc->FreeReg(reg);
        }
    }

    ScopedRegister& operator=(ScopedRegister&& rhs) noexcept {
        if (reg_alloc) {
            reg_alloc->FreeReg(reg);
        }
        reg_alloc = std::exchange(rhs.reg_alloc, nullptr);
        reg = rhs.reg;
        return *this;
    }

    ScopedRegister(ScopedRegister&& rhs) noexcept
        : reg_alloc{std::exchange(rhs.reg_alloc, nullptr)}, reg{rhs.reg} {}

    ScopedRegister& operator=(const ScopedRegister&) = delete;
    ScopedRegister(const ScopedRegister&) = delete;

    RegAlloc* reg_alloc{};
    Register reg;
};

std::string Texture(EmitContext& ctx, IR::TextureInstInfo info,
                    [[maybe_unused]] const IR::Value& index) {
    // FIXME: indexed reads
    if (info.type == TextureType::Buffer) {
        return fmt::format("texture[{}]", ctx.texture_buffer_bindings.at(info.descriptor_index));
    } else {
        return fmt::format("texture[{}]", ctx.texture_bindings.at(info.descriptor_index));
    }
}

std::string Image(EmitContext& ctx, IR::TextureInstInfo info,
                  [[maybe_unused]] const IR::Value& index) {
    // FIXME: indexed reads
    if (info.type == TextureType::Buffer) {
        return fmt::format("image[{}]", ctx.image_buffer_bindings.at(info.descriptor_index));
    } else {
        return fmt::format("image[{}]", ctx.image_bindings.at(info.descriptor_index));
    }
}

std::string_view TextureType(IR::TextureInstInfo info) {
    if (info.is_depth) {
        switch (info.type) {
        case TextureType::Color1D:
            return "SHADOW1D";
        case TextureType::ColorArray1D:
            return "SHADOWARRAY1D";
        case TextureType::Color2D:
            return "SHADOW2D";
        case TextureType::ColorArray2D:
            return "SHADOWARRAY2D";
        case TextureType::Color3D:
            return "SHADOW3D";
        case TextureType::ColorCube:
            return "SHADOWCUBE";
        case TextureType::ColorArrayCube:
            return "SHADOWARRAYCUBE";
        case TextureType::Buffer:
            return "SHADOWBUFFER";
        }
    } else {
        switch (info.type) {
        case TextureType::Color1D:
            return "1D";
        case TextureType::ColorArray1D:
            return "ARRAY1D";
        case TextureType::Color2D:
            return "2D";
        case TextureType::ColorArray2D:
            return "ARRAY2D";
        case TextureType::Color3D:
            return "3D";
        case TextureType::ColorCube:
            return "CUBE";
        case TextureType::ColorArrayCube:
            return "ARRAYCUBE";
        case TextureType::Buffer:
            return "BUFFER";
        }
    }
    throw InvalidArgument("Invalid texture type {}", info.type.Value());
}

std::string Offset(EmitContext& ctx, const IR::Value& offset) {
    if (offset.IsEmpty()) {
        return "";
    }
    return fmt::format(",offset({})", Register{ctx.reg_alloc.Consume(offset)});
}

std::pair<ScopedRegister, ScopedRegister> AllocOffsetsRegs(EmitContext& ctx,
                                                           const IR::Value& offset2) {
    if (offset2.IsEmpty()) {
        return {};
    } else {
        return {ctx.reg_alloc, ctx.reg_alloc};
    }
}

void SwizzleOffsets(EmitContext& ctx, Register off_x, Register off_y, const IR::Value& offset1,
                    const IR::Value& offset2) {
    const Register offsets_a{ctx.reg_alloc.Consume(offset1)};
    const Register offsets_b{ctx.reg_alloc.Consume(offset2)};
    // Input swizzle:  [XYXY] [XYXY]
    // Output swizzle: [XXXX] [YYYY]
    ctx.Add("MOV {}.x,{}.x;"
            "MOV {}.y,{}.z;"
            "MOV {}.z,{}.x;"
            "MOV {}.w,{}.z;"
            "MOV {}.x,{}.y;"
            "MOV {}.y,{}.w;"
            "MOV {}.z,{}.y;"
            "MOV {}.w,{}.w;",
            off_x, offsets_a, off_x, offsets_a, off_x, offsets_b, off_x, offsets_b, off_y,
            offsets_a, off_y, offsets_a, off_y, offsets_b, off_y, offsets_b);
}

std::string GradOffset(const IR::Value& offset) {
    if (offset.IsImmediate()) {
        LOG_WARNING(Shader_GLASM, "Gradient offset is a scalar immediate");
        return "";
    }
    IR::Inst* const vector{offset.InstRecursive()};
    if (!vector->AreAllArgsImmediates()) {
        LOG_WARNING(Shader_GLASM, "Gradient offset vector is not immediate");
        return "";
    }
    switch (vector->NumArgs()) {
    case 1:
        return fmt::format(",({})", static_cast<s32>(vector->Arg(0).U32()));
    case 2:
        return fmt::format(",({},{})", static_cast<s32>(vector->Arg(0).U32()),
                           static_cast<s32>(vector->Arg(1).U32()));
    default:
        throw LogicError("Invalid number of gradient offsets {}", vector->NumArgs());
    }
}

std::pair<std::string, ScopedRegister> Coord(EmitContext& ctx, const IR::Value& coord) {
    if (coord.IsImmediate()) {
        ScopedRegister scoped_reg(ctx.reg_alloc);
        ctx.Add("MOV.U {}.x,{};", scoped_reg.reg, ScalarU32{ctx.reg_alloc.Consume(coord)});
        return {fmt::to_string(scoped_reg.reg), std::move(scoped_reg)};
    }
    std::string coord_vec{fmt::to_string(Register{ctx.reg_alloc.Consume(coord)})};
    if (coord.InstRecursive()->HasUses()) {
        // Move non-dead coords to a separate register, although this should never happen because
        // vectors are only assembled for immediate texture instructions
        ctx.Add("MOV.F RC,{};", coord_vec);
        coord_vec = "RC";
    }
    return {std::move(coord_vec), ScopedRegister{}};
}

void StoreSparse(EmitContext& ctx, IR::Inst* sparse_inst) {
    if (!sparse_inst) {
        return;
    }
    const Register sparse_ret{ctx.reg_alloc.Define(*sparse_inst)};
    ctx.Add("MOV.S {},-1;"
            "MOV.S {}(NONRESIDENT),0;",
            sparse_ret, sparse_ret);
}

std::string_view FormatStorage(ImageFormat format) {
    switch (format) {
    case ImageFormat::Typeless:
        return "U";
    case ImageFormat::R8_UINT:
        return "U8";
    case ImageFormat::R8_SINT:
        return "S8";
    case ImageFormat::R16_UINT:
        return "U16";
    case ImageFormat::R16_SINT:
        return "S16";
    case ImageFormat::R32_UINT:
        return "U32";
    case ImageFormat::R32G32_UINT:
        return "U32X2";
    case ImageFormat::R32G32B32A32_UINT:
        return "U32X4";
    }
    throw InvalidArgument("Invalid image format {}", format);
}

template <typename T>
void ImageAtomic(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord, T value,
                 std::string_view op) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const std::string_view type{TextureType(info)};
    const std::string image{Image(ctx, info, index)};
    const Register ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("ATOMIM.{} {},{},{},{},{};", op, ret, value, coord, image, type);
}

IR::Inst* PrepareSparse(IR::Inst& inst) {
    const auto sparse_inst{inst.GetAssociatedPseudoOperation(IR::Opcode::GetSparseFromOp)};
    if (sparse_inst) {
        sparse_inst->Invalidate();
    }
    return sparse_inst;
}
} // Anonymous namespace

void EmitImageSampleImplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                const IR::Value& coord, Register bias_lc, const IR::Value& offset) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto sparse_inst{PrepareSparse(inst)};
    const std::string_view sparse_mod{sparse_inst ? ".SPARSE" : ""};
    const std::string_view lod_clamp_mod{info.has_lod_clamp ? ".LODCLAMP" : ""};
    const std::string_view type{TextureType(info)};
    const std::string texture{Texture(ctx, info, index)};
    const std::string offset_vec{Offset(ctx, offset)};
    const auto [coord_vec, coord_alloc]{Coord(ctx, coord)};
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (info.has_bias) {
        if (info.type == TextureType::ColorArrayCube) {
            ctx.Add("TXB.F{}{} {},{},{},{},ARRAYCUBE{};", lod_clamp_mod, sparse_mod, ret, coord_vec,
                    bias_lc, texture, offset_vec);
        } else {
            if (info.has_lod_clamp) {
                ctx.Add("MOV.F {}.w,{}.x;"
                        "TXB.F.LODCLAMP{} {},{},{}.y,{},{}{};",
                        coord_vec, bias_lc, sparse_mod, ret, coord_vec, bias_lc, texture, type,
                        offset_vec);
            } else {
                ctx.Add("MOV.F {}.w,{}.x;"
                        "TXB.F{} {},{},{},{}{};",
                        coord_vec, bias_lc, sparse_mod, ret, coord_vec, texture, type, offset_vec);
            }
        }
    } else {
        if (info.has_lod_clamp && info.type == TextureType::ColorArrayCube) {
            ctx.Add("TEX.F.LODCLAMP{} {},{},{},{},ARRAYCUBE{};", sparse_mod, ret, coord_vec,
                    bias_lc, texture, offset_vec);
        } else {
            ctx.Add("TEX.F{}{} {},{},{},{}{};", lod_clamp_mod, sparse_mod, ret, coord_vec, texture,
                    type, offset_vec);
        }
    }
    StoreSparse(ctx, sparse_inst);
}

void EmitImageSampleExplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                const IR::Value& coord, ScalarF32 lod, const IR::Value& offset) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto sparse_inst{PrepareSparse(inst)};
    const std::string_view sparse_mod{sparse_inst ? ".SPARSE" : ""};
    const std::string_view type{TextureType(info)};
    const std::string texture{Texture(ctx, info, index)};
    const std::string offset_vec{Offset(ctx, offset)};
    const auto [coord_vec, coord_alloc]{Coord(ctx, coord)};
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (info.type == TextureType::ColorArrayCube) {
        ctx.Add("TXL.F{} {},{},{},{},ARRAYCUBE{};", sparse_mod, ret, coord_vec, lod, texture,
                offset_vec);
    } else {
        ctx.Add("MOV.F {}.w,{};"
                "TXL.F{} {},{},{},{}{};",
                coord_vec, lod, sparse_mod, ret, coord_vec, texture, type, offset_vec);
    }
    StoreSparse(ctx, sparse_inst);
}

void EmitImageSampleDrefImplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                    const IR::Value& coord, const IR::Value& dref,
                                    const IR::Value& bias_lc, const IR::Value& offset) {
    // Allocate early to avoid aliases
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    ScopedRegister staging;
    if (info.type == TextureType::ColorArrayCube) {
        staging = ScopedRegister{ctx.reg_alloc};
    }
    const ScalarF32 dref_val{ctx.reg_alloc.Consume(dref)};
    const Register bias_lc_vec{ctx.reg_alloc.Consume(bias_lc)};
    const auto sparse_inst{PrepareSparse(inst)};
    const std::string_view sparse_mod{sparse_inst ? ".SPARSE" : ""};
    const std::string_view type{TextureType(info)};
    const std::string texture{Texture(ctx, info, index)};
    const std::string offset_vec{Offset(ctx, offset)};
    const auto [coord_vec, coord_alloc]{Coord(ctx, coord)};
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (info.has_bias) {
        if (info.has_lod_clamp) {
            switch (info.type) {
            case TextureType::Color1D:
            case TextureType::ColorArray1D:
            case TextureType::Color2D:
                ctx.Add("MOV.F {}.z,{};"
                        "MOV.F {}.w,{}.x;"
                        "TXB.F.LODCLAMP{} {},{},{}.y,{},{}{};",
                        coord_vec, dref_val, coord_vec, bias_lc_vec, sparse_mod, ret, coord_vec,
                        bias_lc_vec, texture, type, offset_vec);
                break;
            case TextureType::ColorArray2D:
            case TextureType::ColorCube:
                ctx.Add("MOV.F {}.w,{};"
                        "TXB.F.LODCLAMP{} {},{},{},{},{}{};",
                        coord_vec, dref_val, sparse_mod, ret, coord_vec, bias_lc_vec, texture, type,
                        offset_vec);
                break;
            default:
                throw NotImplementedException("Invalid type {} with bias and lod clamp",
                                              info.type.Value());
            }
        } else {
            switch (info.type) {
            case TextureType::Color1D:
            case TextureType::ColorArray1D:
            case TextureType::Color2D:
                ctx.Add("MOV.F {}.z,{};"
                        "MOV.F {}.w,{}.x;"
                        "TXB.F{} {},{},{},{}{};",
                        coord_vec, dref_val, coord_vec, bias_lc_vec, sparse_mod, ret, coord_vec,
                        texture, type, offset_vec);
                break;
            case TextureType::ColorArray2D:
            case TextureType::ColorCube:
                ctx.Add("MOV.F {}.w,{};"
                        "TXB.F{} {},{},{},{},{}{};",
                        coord_vec, dref_val, sparse_mod, ret, coord_vec, bias_lc_vec, texture, type,
                        offset_vec);
                break;
            case TextureType::ColorArrayCube:
                ctx.Add("MOV.F {}.x,{};"
                        "MOV.F {}.y,{}.x;"
                        "TXB.F{} {},{},{},{},{}{};",
                        staging.reg, dref_val, staging.reg, bias_lc_vec, sparse_mod, ret, coord_vec,
                        staging.reg, texture, type, offset_vec);
                break;
            default:
                throw NotImplementedException("Invalid type {}", info.type.Value());
            }
        }
    } else {
        if (info.has_lod_clamp) {
            if (info.type != TextureType::ColorArrayCube) {
                const bool w_swizzle{info.type == TextureType::ColorArray2D ||
                                     info.type == TextureType::ColorCube};
                const char dref_swizzle{w_swizzle ? 'w' : 'z'};
                ctx.Add("MOV.F {}.{},{};"
                        "TEX.F.LODCLAMP{} {},{},{},{},{}{};",
                        coord_vec, dref_swizzle, dref_val, sparse_mod, ret, coord_vec, bias_lc_vec,
                        texture, type, offset_vec);
            } else {
                ctx.Add("MOV.F {}.x,{};"
                        "MOV.F {}.y,{};"
                        "TEX.F.LODCLAMP{} {},{},{},{},{}{};",
                        staging.reg, dref_val, staging.reg, bias_lc_vec, sparse_mod, ret, coord_vec,
                        staging.reg, texture, type, offset_vec);
            }
        } else {
            if (info.type != TextureType::ColorArrayCube) {
                const bool w_swizzle{info.type == TextureType::ColorArray2D ||
                                     info.type == TextureType::ColorCube};
                const char dref_swizzle{w_swizzle ? 'w' : 'z'};
                ctx.Add("MOV.F {}.{},{};"
                        "TEX.F{} {},{},{},{}{};",
                        coord_vec, dref_swizzle, dref_val, sparse_mod, ret, coord_vec, texture,
                        type, offset_vec);
            } else {
                ctx.Add("TEX.F{} {},{},{},{},{}{};", sparse_mod, ret, coord_vec, dref_val, texture,
                        type, offset_vec);
            }
        }
    }
    StoreSparse(ctx, sparse_inst);
}

void EmitImageSampleDrefExplicitLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                                    const IR::Value& coord, const IR::Value& dref,
                                    const IR::Value& lod, const IR::Value& offset) {
    // Allocate early to avoid aliases
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    ScopedRegister staging;
    if (info.type == TextureType::ColorArrayCube) {
        staging = ScopedRegister{ctx.reg_alloc};
    }
    const ScalarF32 dref_val{ctx.reg_alloc.Consume(dref)};
    const ScalarF32 lod_val{ctx.reg_alloc.Consume(lod)};
    const auto sparse_inst{PrepareSparse(inst)};
    const std::string_view sparse_mod{sparse_inst ? ".SPARSE" : ""};
    const std::string_view type{TextureType(info)};
    const std::string texture{Texture(ctx, info, index)};
    const std::string offset_vec{Offset(ctx, offset)};
    const auto [coord_vec, coord_alloc]{Coord(ctx, coord)};
    const Register ret{ctx.reg_alloc.Define(inst)};
    switch (info.type) {
    case TextureType::Color1D:
    case TextureType::ColorArray1D:
    case TextureType::Color2D:
        ctx.Add("MOV.F {}.z,{};"
                "MOV.F {}.w,{};"
                "TXL.F{} {},{},{},{}{};",
                coord_vec, dref_val, coord_vec, lod_val, sparse_mod, ret, coord_vec, texture, type,
                offset_vec);
        break;
    case TextureType::ColorArray2D:
    case TextureType::ColorCube:
        ctx.Add("MOV.F {}.w,{};"
                "TXL.F{} {},{},{},{},{}{};",
                coord_vec, dref_val, sparse_mod, ret, coord_vec, lod_val, texture, type,
                offset_vec);
        break;
    case TextureType::ColorArrayCube:
        ctx.Add("MOV.F {}.x,{};"
                "MOV.F {}.y,{};"
                "TXL.F{} {},{},{},{},{}{};",
                staging.reg, dref_val, staging.reg, lod_val, sparse_mod, ret, coord_vec,
                staging.reg, texture, type, offset_vec);
        break;
    default:
        throw NotImplementedException("Invalid type {}", info.type.Value());
    }
    StoreSparse(ctx, sparse_inst);
}

void EmitImageGather(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                     const IR::Value& coord, const IR::Value& offset, const IR::Value& offset2) {
    // Allocate offsets early so they don't overwrite any consumed register
    const auto [off_x, off_y]{AllocOffsetsRegs(ctx, offset2)};
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const char comp{"xyzw"[info.gather_component]};
    const auto sparse_inst{PrepareSparse(inst)};
    const std::string_view sparse_mod{sparse_inst ? ".SPARSE" : ""};
    const std::string_view type{TextureType(info)};
    const std::string texture{Texture(ctx, info, index)};
    const Register coord_vec{ctx.reg_alloc.Consume(coord)};
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (offset2.IsEmpty()) {
        const std::string offset_vec{Offset(ctx, offset)};
        ctx.Add("TXG.F{} {},{},{}.{},{}{};", sparse_mod, ret, coord_vec, texture, comp, type,
                offset_vec);
    } else {
        SwizzleOffsets(ctx, off_x.reg, off_y.reg, offset, offset2);
        ctx.Add("TXGO.F{} {},{},{},{},{}.{},{};", sparse_mod, ret, coord_vec, off_x.reg, off_y.reg,
                texture, comp, type);
    }
    StoreSparse(ctx, sparse_inst);
}

void EmitImageGatherDref(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                         const IR::Value& coord, const IR::Value& offset, const IR::Value& offset2,
                         const IR::Value& dref) {
    // FIXME: This instruction is not working as expected

    // Allocate offsets early so they don't overwrite any consumed register
    const auto [off_x, off_y]{AllocOffsetsRegs(ctx, offset2)};
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto sparse_inst{PrepareSparse(inst)};
    const std::string_view sparse_mod{sparse_inst ? ".SPARSE" : ""};
    const std::string_view type{TextureType(info)};
    const std::string texture{Texture(ctx, info, index)};
    const Register coord_vec{ctx.reg_alloc.Consume(coord)};
    const ScalarF32 dref_value{ctx.reg_alloc.Consume(dref)};
    const Register ret{ctx.reg_alloc.Define(inst)};
    std::string args;
    switch (info.type) {
    case TextureType::Color2D:
        ctx.Add("MOV.F {}.z,{};", coord_vec, dref_value);
        args = fmt::to_string(coord_vec);
        break;
    case TextureType::ColorArray2D:
    case TextureType::ColorCube:
        ctx.Add("MOV.F {}.w,{};", coord_vec, dref_value);
        args = fmt::to_string(coord_vec);
        break;
    case TextureType::ColorArrayCube:
        args = fmt::format("{},{}", coord_vec, dref_value);
        break;
    default:
        throw NotImplementedException("Invalid type {}", info.type.Value());
    }
    if (offset2.IsEmpty()) {
        const std::string offset_vec{Offset(ctx, offset)};
        ctx.Add("TXG.F{} {},{},{},{}{};", sparse_mod, ret, args, texture, type, offset_vec);
    } else {
        SwizzleOffsets(ctx, off_x.reg, off_y.reg, offset, offset2);
        ctx.Add("TXGO.F{} {},{},{},{},{},{};", sparse_mod, ret, args, off_x.reg, off_y.reg, texture,
                type);
    }
    StoreSparse(ctx, sparse_inst);
}

void EmitImageFetch(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                    const IR::Value& coord, const IR::Value& offset, ScalarS32 lod, ScalarS32 ms) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto sparse_inst{PrepareSparse(inst)};
    const std::string_view sparse_mod{sparse_inst ? ".SPARSE" : ""};
    const std::string_view type{TextureType(info)};
    const std::string texture{Texture(ctx, info, index)};
    const std::string offset_vec{Offset(ctx, offset)};
    const auto [coord_vec, coord_alloc]{Coord(ctx, coord)};
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (info.type == TextureType::Buffer) {
        ctx.Add("TXF.F{} {},{},{},{}{};", sparse_mod, ret, coord_vec, texture, type, offset_vec);
    } else if (ms.type != Type::Void) {
        ctx.Add("MOV.S {}.w,{};"
                "TXFMS.F{} {},{},{},{}{};",
                coord_vec, ms, sparse_mod, ret, coord_vec, texture, type, offset_vec);
    } else {
        ctx.Add("MOV.S {}.w,{};"
                "TXF.F{} {},{},{},{}{};",
                coord_vec, lod, sparse_mod, ret, coord_vec, texture, type, offset_vec);
    }
    StoreSparse(ctx, sparse_inst);
}

void EmitImageQueryDimensions(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                              ScalarS32 lod) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const std::string texture{Texture(ctx, info, index)};
    const std::string_view type{TextureType(info)};
    ctx.Add("TXQ {},{},{},{};", inst, lod, texture, type);
}

void EmitImageQueryLod(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const std::string texture{Texture(ctx, info, index)};
    const std::string_view type{TextureType(info)};
    ctx.Add("LOD.F {},{},{},{};", inst, coord, texture, type);
}

void EmitImageGradient(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                       const IR::Value& coord, const IR::Value& derivatives,
                       const IR::Value& offset, const IR::Value& lod_clamp) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    ScopedRegister dpdx, dpdy;
    const bool multi_component{info.num_derivates > 1 || info.has_lod_clamp};
    if (multi_component) {
        // Allocate this early to avoid aliasing other registers
        dpdx = ScopedRegister{ctx.reg_alloc};
        dpdy = ScopedRegister{ctx.reg_alloc};
    }
    const auto sparse_inst{PrepareSparse(inst)};
    const std::string_view sparse_mod{sparse_inst ? ".SPARSE" : ""};
    const std::string_view type{TextureType(info)};
    const std::string texture{Texture(ctx, info, index)};
    const std::string offset_vec{GradOffset(offset)};
    const Register coord_vec{ctx.reg_alloc.Consume(coord)};
    const Register derivatives_vec{ctx.reg_alloc.Consume(derivatives)};
    const Register ret{ctx.reg_alloc.Define(inst)};
    if (multi_component) {
        ctx.Add("MOV.F {}.x,{}.x;"
                "MOV.F {}.y,{}.z;"
                "MOV.F {}.x,{}.y;"
                "MOV.F {}.y,{}.w;",
                dpdx.reg, derivatives_vec, dpdx.reg, derivatives_vec, dpdy.reg, derivatives_vec,
                dpdy.reg, derivatives_vec);
        if (info.has_lod_clamp) {
            const ScalarF32 lod_clamp_value{ctx.reg_alloc.Consume(lod_clamp)};
            ctx.Add("MOV.F {}.w,{};"
                    "TXD.F.LODCLAMP{} {},{},{},{},{},{}{};",
                    dpdy.reg, lod_clamp_value, sparse_mod, ret, coord_vec, dpdx.reg, dpdy.reg,
                    texture, type, offset_vec);
        } else {
            ctx.Add("TXD.F{} {},{},{},{},{},{}{};", sparse_mod, ret, coord_vec, dpdx.reg, dpdy.reg,
                    texture, type, offset_vec);
        }
    } else {
        ctx.Add("TXD.F{} {},{},{}.x,{}.y,{},{}{};", sparse_mod, ret, coord_vec, derivatives_vec,
                derivatives_vec, texture, type, offset_vec);
    }
    StoreSparse(ctx, sparse_inst);
}

void EmitImageRead(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const auto sparse_inst{PrepareSparse(inst)};
    const std::string_view format{FormatStorage(info.image_format)};
    const std::string_view sparse_mod{sparse_inst ? ".SPARSE" : ""};
    const std::string_view type{TextureType(info)};
    const std::string image{Image(ctx, info, index)};
    const Register ret{ctx.reg_alloc.Define(inst)};
    ctx.Add("LOADIM.{}{} {},{},{},{};", format, sparse_mod, ret, coord, image, type);
    StoreSparse(ctx, sparse_inst);
}

void EmitImageWrite(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                    Register color) {
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const std::string_view format{FormatStorage(info.image_format)};
    const std::string_view type{TextureType(info)};
    const std::string image{Image(ctx, info, index)};
    ctx.Add("STOREIM.{} {},{},{},{};", format, image, color, coord, type);
}

void EmitImageAtomicIAdd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                           ScalarU32 value) {
    ImageAtomic(ctx, inst, index, coord, value, "ADD.U32");
}

void EmitImageAtomicSMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                           ScalarS32 value) {
    ImageAtomic(ctx, inst, index, coord, value, "MIN.S32");
}

void EmitImageAtomicUMin32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                           ScalarU32 value) {
    ImageAtomic(ctx, inst, index, coord, value, "MIN.U32");
}

void EmitImageAtomicSMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                           ScalarS32 value) {
    ImageAtomic(ctx, inst, index, coord, value, "MAX.S32");
}

void EmitImageAtomicUMax32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                           ScalarU32 value) {
    ImageAtomic(ctx, inst, index, coord, value, "MAX.U32");
}

void EmitImageAtomicInc32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                          ScalarU32 value) {
    ImageAtomic(ctx, inst, index, coord, value, "IWRAP.U32");
}

void EmitImageAtomicDec32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                          ScalarU32 value) {
    ImageAtomic(ctx, inst, index, coord, value, "DWRAP.U32");
}

void EmitImageAtomicAnd32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                          ScalarU32 value) {
    ImageAtomic(ctx, inst, index, coord, value, "AND.U32");
}

void EmitImageAtomicOr32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                         ScalarU32 value) {
    ImageAtomic(ctx, inst, index, coord, value, "OR.U32");
}

void EmitImageAtomicXor32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index, Register coord,
                          ScalarU32 value) {
    ImageAtomic(ctx, inst, index, coord, value, "XOR.U32");
}

void EmitImageAtomicExchange32(EmitContext& ctx, IR::Inst& inst, const IR::Value& index,
                               Register coord, ScalarU32 value) {
    ImageAtomic(ctx, inst, index, coord, value, "EXCH.U32");
}

void EmitBindlessImageSampleImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageSampleExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageSampleDrefImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageSampleDrefExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageGather(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageGatherDref(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageFetch(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageQueryDimensions(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageQueryLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageGradient(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageRead(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageWrite(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageSampleImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageSampleExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageSampleDrefImplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageSampleDrefExplicitLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageGather(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageGatherDref(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageFetch(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageQueryDimensions(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageQueryLod(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageGradient(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageRead(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageWrite(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicIAdd32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicSMin32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicUMin32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicSMax32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicUMax32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicInc32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicDec32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicAnd32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicOr32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicXor32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBindlessImageAtomicExchange32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicIAdd32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicSMin32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicUMin32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicSMax32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicUMax32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicInc32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicDec32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicAnd32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicOr32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicXor32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

void EmitBoundImageAtomicExchange32(EmitContext&) {
    throw LogicError("Unreachable instruction");
}

} // namespace Shader::Backend::GLASM
