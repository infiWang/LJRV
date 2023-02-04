/*
** RISC-V instruction emitter.
** Copyright (C) 2005-2022 Mike Pall. See Copyright Notice in luajit.h
*/

#include "lj_target.h"
#include <stdint.h>
static intptr_t get_k64val(ASMState *as, IRRef ref)
{
  IRIns *ir = IR(ref);
  if (ir->o == IR_KINT64) {
    return (intptr_t)ir_kint64(ir)->u64;
  } else if (ir->o == IR_KGC) {
    return (intptr_t)ir_kgc(ir);
  } else if (ir->o == IR_KPTR || ir->o == IR_KKPTR) {
    return (intptr_t)ir_kptr(ir);
  } else {
    lj_assertA(ir->o == IR_KINT || ir->o == IR_KNULL,
               "bad 64 bit const IR op %d", ir->o);
    return ir->i;  /* Sign-extended. */
  }
}

#define get_kval(as, ref)       get_k64val(as, ref)

/* -- Emit basic instructions --------------------------------------------- */

static void emit_r(ASMState *as, RISCVIns riscvi, Reg rd, Reg rs1, Reg rs2)
{
  *--as->mcp = riscvi | RISCVF_D(rd) | RISCVF_S1(rs1) | RISCVF_S2(rs2);
}

#define emit_ds(as, riscvi, rd, rs1)         emit_r(as, riscvi, rd, rs1, 0)
#define emit_ds2(as, riscvi, rd, rs2)         emit_r(as, riscvi, rd, 0, rs1)
#define emit_ds1s2(as, riscvi, rd, rs1, rs2)         emit_r(as, riscvi, rd, rs1, rs2)

static void emit_r4(ASMState *as, RISCVIns riscvi, Reg rd, Reg rs1, Reg rs2, Reg rs3)
{
  *--as->mcp = riscvi | RISCVF_D(rd) | RISCVF_S1(rs1) | RISCVF_S2(rs2) | RISCVF_S3(rs3);
}

#define emit_ds1s2s3(as, riscvi, rd, rs1, rs2, rs3)         emit_r4(as, riscvi, rd, rs1, rs2, rs3)

static void emit_i(ASMState *as, RISCVIns riscvi, Reg rd, Reg rs1, int32_t i)
{
  *--as->mcp = riscvi | RISCVF_RD(rd) | RISCVF_RS1(rs1) | RISCVF_IMMI(i & 0xfff);
}

#define emit_dsi(as, riscvi, rd, rs1, i)     emit_i(as, riscvi, rd, rs1, i)
#define emit_dsshamt(as, riscvi, rd, rs1, i) emit_i(as, riscvi, rd, rs1, i&0x3f)

static void emit_s(ASMState *as, RISCVIns riscvi, Reg rs1, Reg rs2, int32_t i)
{
  *--as->mcp = riscvi | RISCVF_RS1(rs1) | RISCVF_RS2(rs2) | RISCVF_IMMS(i & 0xfff);
}

#define emit_s1s2i(as, riscvi, rs1, rs2, i)  emit_s(as, riscvi, rs1, rs2, i)

static void emit_b(ASMState *as, RISCVIns riscvi, Reg rs1, Reg rs2, int32_t i)
{
  *--as->mcp = riscvi | RISCVF_RS1(rs1) | RISCVF_RS2(rs2) | RISCVF_IMMB(i & 0x1ffe);
}

static void emit_u(ASMState *as, RISCVIns riscvi, Reg rd, int32_t i)
{
  *--as->mcp = riscvi | RISCVF_RD(rd) | RISCVF_IMMU(i & 0xfffff);
}

static void emit_j(ASMState *as, RISCVIns riscvi, Reg rd, int32_t i)
{
  *--as->mcp = riscvi | RISCVF_RD(rd) | RISCVF_IMMJ(i & 0x1fffffe);
}

static void emit_roti(ASMState *as, RISCVIns riscvi, Reg rd, Reg rs1, int32_t shamt, RegSet allow)
{
  if (as->flags & JIT_F_RVB) {
    emit_dsshamt(as, riscvi, rd, rs1, shamt);
  } else {
    RISCVIns ai, bi;
    int32_t shwid, shmsk;
    Reg tmp = ra_scratch(as, rset_exclude(allow, rd));
    switch (riscvi) {
      case RISCVI_RORI:
        ai = RISCVI_SRLI, bi = RISCVI_SLLI;
        shwid = 64, shmsk = 63;
        break;
      case RISCVI_RORIW:
        ai = RISCVI_SRLIW, bi = RISCVI_SLLIW;
        shwid = 32, shmsk = 31;
        break;
      default:
        lj_assertA(0, "invalid roti op");
        return;
    }
    emit_ds1s2(as, RISCVI_OR, rd, rd, tmp);
    emit_dsshamt(as, bi, tmp, rs1, (shwid - shamt)&shmsk);
    emit_dsshamt(as, ai, rd, rs1, shamt&shmsk);
  }
}

static void emit_rot(ASMState *as, RISCVIns riscvi, Reg rd, Reg rs1, Reg rs2, RegSet allow)
{
  if (as->flags & JIT_F_RVB) {
    emit_ds1s2(as, riscvi, rd, rs1, rs2);
  } else {
    RISCVIns sai, sbi;
    allow = rset_exclude(allow, RID2RSET(rd));
    allow = rset_exclude(allow, RID2RSET(rs1));
    allow = rset_exclude(allow, RID2RSET(rs2));
    Reg nsh = ra_scratch(as, allow),
        tmp = ra_scratch(as, rset_exclude(allow, (nsh)));
    switch (riscvi) {
      case RISCVI_ROL:
        ai = RISCVI_SLL, bi = RISCVI_SRL;
        break;
      case RISCVI_ROR:
        ai = RISCVI_SRL,  bi = RISCVI_SLL;
        break;
      case RISCVI_ROLW:
        ai = RISCVI_SLLW, bi = RISCVI_SRLW;
        break;
      case RISCVI_RORW:
        ai = RISCVI_SRLW, bi = RISCVI_SLLW;
        break;
      default:
        lj_assertA(0, "invalid rot op");
        return;
    }
    emit_ds1s2(as, RISCVI_OR, rd, rd, tmp);
    emit_ds1s2(as, sbi, rd, rs1, nsh);
    emit_ds1s2(as, sai, tmp, rs1, rs2);
    emit_ds2(as, RISCVI_NEG, nsh, rs2);
  }
}

static void emit_ext(ASMState *as, RISCVIns riscvi, Reg rd, Reg rs1)
{
  if (as->flags & JIT_F_RVB) {
    emit_ds(as, riscvi, rd, rs1);
  } else {
    RISCVIns sli, sri;
    int32_t slamt, sramt;
    switch (riscvi) {
      case RISCVI_ZEXT_B:
      case RISCVI_SEXT_W:
        emit_ds(as, riscvi, rd, rs1);
        return;
      case RISCVI_ZEXT_H:
        sli = RISCVI_SLLI;
        sri = RISCVI_SRLI;
        slamt = sramt = 48;
        break;
      case RISCV_ZEXT_W:
        sli = RISCVI_SLLI;
        sri = RISCVI_SRLI;
        slamt = sramt = 32;
        break;
      case RISCVI_SEXT_B:
        sli = RISCVI_SLLI;
        sri = RISCVI_SRAI;
        slamt = sramt = 56;
        break;
      case RISCVI_SEXT_H:
        sli = RISCVI_SLLI;
        sri = RISCVI_SRAI;
        slamt = sramt = 48;
        break;
      default:
        lj_assertA(0, "invalid ext op");
    }
    emit_dsshamt(as, sri, rd, rd, sramt);   
    emit_dsshamt(as, sli, rd, rs1, slamt);
  }
}

#define checki12(x)	RISCVF_SIMM_OK(x, 12)
#define checku12(x)	((x) == ((x) & 0xfff))
#define checki20(x)	RISCVF_SIMM_OK(x, 20)
#define checku20(x)	((x) == ((x) & 0xfffff))
#define checki21(x)	RISCVF_SIMM_OK(x, 20)
#define checki32(x) RISCVF_SIMM_OK(x, 32)
#define checki33(x) RISCVF_SIMM_OK(x, 33)

static Reg ra_allock(ASMState *as, intptr_t k, RegSet allow);
static void ra_allockreg(ASMState *as, intptr_t k, Reg r);
static Reg ra_scratch(ASMState *as, RegSet allow);

static void emit_loadk12(ASMState *as, Reg rd, int32_t i)
{
  emit_i_dsi(as, RISCVI_ADDI, rd, rd, i);
}

static void emit_loadk20(ASMState *as, Reg rd, int32_t i)
{
  emit_dsshamt(as, RISCVI_SRAI_D, rd, rd, 12);
  emit_u(as, RISCVI_LUI, rd, (i&0xfffff));
}

static void emit_loadk32(ASMState *as, Reg rd, int32_t i)
{
  if (checki12(i)) {
    emit_di12(as, rd, i);
  } else {
    emit_i_dsi(as, RISCVI_ADDI, rd, rd, RISCVF_LO(i));
    emit_u_di(as, RISCVI_LUI, rd, RISCVF_HI(i));
  }
}

/* -- Emit loads/stores --------------------------------------------------- */

/* Prefer rematerialization of BASE/L from global_State over spills. */
#define emit_canremat(ref)	((ref) <= REF_BASE)


/* Load a 32 bit constant into a GPR. */
#define emit_loadi(as, r, i)	emit_loadk32(as, r, i);

/* Load a 64 bit constant into a GPR. */
static void emit_loadu64(ASMState *as, Reg r, uint64_t u64)
{
  if (checki32((int64_t)u64)) {
    emit_loadk32(as, r, (int32_t)u64);
  } else {
    emit_dsi(as, RISCVI_ADDI, r, r, u64 & 0x3ff);
    emit_dsshamt(as, RISCVI_SLLI, r, r, 10);
    emit_dsi(as, RISCVI_ADDI, r, r, (u64 >> 10) & 0x7ff);
    emit_dsshamt(as, RISCVI_SLLI, r, r, 11);
    emit_dsi(as, RISCVI_ADDI, r, r, (u64 >> 21) & 0x7ff);
    emit_dsshamt(as, RISCVI_SLLI, r, r, 11);
    emit_loadk32(as, r, r, (u64 >> 32) & 0xffffffff);
  }
}

#define emit_loada(as, r, addr)	emit_loadu64(as, (r), u64ptr((addr)))

/* Get/set from constant pointer. */
static void emit_lsptr(ASMState *as, RISCVIns riscvi, Reg r, void *p, RegSet allow)
{
  intptr_t jgl = (intptr_t)(J2G(as->J));
  intptr_t i = (intptr_t)(p);
  Reg base;
  // if ((uint32_t)(i-jgl) < 65536) {
  //   i = i-jgl-32768;
  //   base = RID_JGL;
  // } else {
    base = ra_allock(as, RISCVF_W_HI(i), allow);
  // }
  emit_lso(as, riscvi, r, base, i);
}

/* Load 64 bit IR constant into register. */
static void emit_loadk64(ASMState *as, Reg r, IRIns *ir)
{
  const uint64_t *k = &ir_k64(ir)->u64;
  Reg r64 = r;
  if (rset_test(RSET_FPR, r)) {
    r64 = RID_TMP;
    emit_ds(as, RISCVI_FMV_D_X, r, r64);
  }
  if ((uint32_t)((intptr_t)k-(intptr_t)J2G(as->J)) < 65536)
    emit_lsptr(as, RISCVI_LD, r64, (void *)k, 0);	/*To copy a doubleword from a GPR to an FPR*/
  else
    emit_loadu64(as, r64, *k);
}

/* Get/set global_State fields. */
static void emit_lsglptr(ASMState *as, RISCVIns riscvi, Reg r, int32_t ofs, RegSet allow)
{
  Reg base;
  // if ((uint32_t)(ofs) < 65536) {
  //   ofs = ofs-32768;
  //   base = RID_JGL;
  // } else {
  //   base = ra_allock(as, RISCVF_W_HI(i), allow);
  // }
  // emit_lso(as, riscvi, r, base, (ofs-32768));
  emit_lso(as, riscvi, r, RID_JGL, ofs-32768);
}

// TODO: migrate JGL to GL? RV LSO offset is rather short.
#define emit_getgl(as, r, field) \
  emit_lsglptr(as, RISCVI_LD, (r), (int32_t)offsetof(global_State, field))
#define emit_setgl(as, r, field) \
  emit_lsglptr(as, RISCVI_SD, (r), (int32_t)offsetof(global_State, field))
// #define emit_getgl(as, r, field) \
//   emit_lsptr(as, RISCVI_LD, (r), (void *)&J2G(as->J)->field)
// #define emit_setgl(as, r, field) \
//   emit_lsptr(as, RISCVI_SD, (r), (void *)&J2G(as->J)->field)

/* Trace number is determined from per-trace exit stubs. */
#define emit_setvmstate(as, i)		UNUSED(i)

/* -- Emit control-flow instructions -------------------------------------- */

/* Label for internal jumps. */
typedef MCode *MCLabel;

/* Return label pointing to current PC. */
#define emit_label(as)		((as)->mcp)

static void emit_branch(ASMState *as, RISCVIns riscvi, Reg rs1, Reg rs2, MCode *target)
{
  MCode *p = as->mcp;
  ptrdiff_t delta = target - p;
  // lj_assertA(((delta + 0x10000) >> 13) == 0, "branch target out of range"); /* B */
  lj_assertA((((delta-4) + 0x100000) >> 21) == 0, "branch target out of range"); /* ^B+J */
  if (checki13(delta)) {
    *--p = riscvi | RISCVF_S1(rs1) | RISCVF_S2(rs2) | RISCVF_IMMB(delta);
  } else {
    *--p = RISCVI_JAL | RISCVF_D(RID_ZERO) | RISCVF_IMMJ(delta-4); /* Poorman's trampoline */
    *--p = (riscvi^0x00001000) | RISCVF_S1(rs1) | RISCVF_S2(rs2) | RISCVF_IMMB(8);
  }
  as->mcp = p;
}

static void emit_jmp(ASMState *as, MCode *target)
{
  // TODO: allocate RID_CFUNCADDR like MIPS call
  MCode *p = as->mcp;
  ptrdiff_t delta = target - p;
  // lj_assertA(((delta + 0x100000) >> 21) == 0, "jump target out of range"); /* J */
  lj_assertA(checki32(delta) == 0, "jump target out of range"); /* AUIPC+JALR */
  if (checki21(delta)) {
    emit_j_di(as, RISCVI_JAL, RID_ZERO, delta);
  } else {
    Reg tmp = ra_scratch(as, RSET_GPR);
    emit_dsi(as, RISCVI_JALR, RID_ZERO, RID_CFUNCADDR, RISCVF_LO(delta));
    emit_u(as, RISCVI_AUIPC, RID_CFUNCADDR, RISCVF_HI(delta));
  }
}

#define emit_mv(as, dst, src) \
  emit_ds(as, RISCVI_MV, (dst), (src))

static void emit_call(ASMState *as, void *target, int needcfa)
{
  // TODO: allocate RID_CFUNCADDR like MIPS
  MCode *p = as->mcp;
  ptrdiff_t delta = (char *)target - ((char *)p);
  if (checki21(delta)) {
    *--p = RISCVI_JAL | RISCVF_D(RID_RA) | RISCVF_IMMJ(delta);
  } else if (checki32(delta)) {
    *--p = RISCVI_JALR | RISCVF_D(RID_RA) | RISCVF_S1(RID_CFUNADDR) | RISCVF_IMMI(RISCVF_LI_LO(delta));
    *--p = RISCVI_AUIPC | RISCVF_D(RID_CFUNCADDR) | RISCVF_IMMU(RISCVF_LI_HI(delta));
    needcfa = 1;
  } else {
    *--p = RISCVI_JALR | RISCVF_D(RID_RA) | RISCVF_S1(RID_CFUNCADDR) | RISCVF_IMMI(0);
    needcfa = 2;
  }
  if (needcfa > 1)
    // ra_allockreg(as, (intptr_t)target, RID_CFUNCADDR);
    emit_loada(as, RID_CFUNCADDR, (intptr_t)target); 
  else if (needcfa > 0)
    // ra_scratch(as, RID_CFUNCADDR);
}

/* -- Emit generic operations --------------------------------------------- */

/* Generic move between two regs. */
static void emit_movrr(ASMState *as, IRIns *ir, Reg dst, Reg src)
{
  if (src < RID_MAX_GPR && dst < RID_MAX_GPR)
    emit_mv(as, dst, src);
  else if (src < RID_MAX_GPR)
    emit_dsi(as, irt_isnum(ir->t) ? RISCVI_FMV_D_X : RISCVI_FMV_S_X, dst, src, 0);
  else if (dst < RID_MAX_GPR)
    emit_dsi(as, irt_isnum(ir->t) ? RISCVI_FMV_X_D : RISCVI_FMV_X_S, dst, src, 0);
  else
    emit_dsi(as, irt_isnum(ir->t) ? RISCVI_FMV_D : RISCVI_FMV_S, dst, src, 0);
}

/* Emit an arithmetic operation with a constant operand. */
static void emit_opk(ASMState *as, RISCVIns riscvi, Reg dest, Reg src, int32_t i)
{
  if ((((riscvi != RISCVI_ORI) && (riscvi != RISCVI_XORI)) && checki12(i)) ||
      (((riscvi == RISCVI_ORI) || (riscvi == RISCVI_XORI)) && checki12(i << 1))) {
    emit_dsi(as, riscvi, dest, src, i);
  } else {
    switch (riscvi) {
      case RISCVI_ADDI: riscvi = i >= 0 ? RISCVI_ADD : RISCVI_SUB; break;
      case RISCVI_XORI: riscvi = RISCVI_XOR; break;
      case RISCVI_ORI: riscvi = RISCVI_OR; break;
      case RISCVI_ANDI: riscvi = RISCVI_AND; break;
      default: lj_assertA(0, "NYI arithmetic RISCVIns"); return;
    }
    emit_ds1s2(as, riscvi, dest, src, RID_TMP);
    emit_loadi(as, RID_TMP, i);
  }
}

static void emit_lso(ASMState *as, RISCVIns riscvi, Reg dest, Reg src, int64_t ofs)
{
  // lj_assertA(checki12(ofs), "load/store offset %d out of range", ofs);
  // TODO: Rewrite referring to arm and arm64, allocate a register for ofs
  switch (riscvi) {
    case RISCVI_LD: case RISCVI_LW: case RISCVI_LH: case RISCVI_LB:
    case RISCVI_LWU: case RISCVI_LHU: case RISCVI_LBU:
      if (checki12(i))
        emit_dsi(as, riscvi, dest, src, ofs);
      else
        emit_dsi(as, riscvi, dest, dest, 0);
        emit_ds1s2(as, dest, dest, src);
        emit_loadi(as, dest, i);
      break;
    case RISCVI_SD: case RISCVI_SW: case RISCVI_SH: case RISCVI_SB:
      if (checki12(i))
        emit_s1s2i(as, riscvi, dest, src, i);
      else
        emit_s1s2i(as, riscvi, dest, RID_CFUNCADDR, 0);
        emit_ds1s2(as, RID_CFUNCADDR, RID_CFUNCADDR, src);
        emit_loadi(as, RID_CFUNCADDR, i);
      break;
    default: lj_assertA(0, "invalid lso"); break;
  }
}

/* Generic load of register with base and (small) offset address. */
static void emit_loadofs(ASMState *as, IRIns *ir, Reg r, Reg base, int32_t ofs)
{
  if (r < RID_MAX_GPR)
    emit_lso(as, irt_is64(ir->t) ? RISCVI_LD : RISCVI_LW, r, base, ofs);
  else
    emit_lso(as, irt_isnum(ir->t) ? RISCVI_FLD : RISCVI_FLW, r, base, ofs);
}

/* Generic store of register with base and (small) offset address. */
static void emit_storeofs(ASMState *as, IRIns *ir, Reg r, Reg base, int32_t ofs)
{
  if (r < RID_MAX_GPR)
    emit_lso(as, irt_is64(ir->t) ? RISCVI_SD : RISCVI_SW, r, base, ofs);
  else
    emit_lso(as, irt_isnum(ir->t) ? RISCVI_FSD : RISCVI_FSW, r, base, ofs);
}

/* Add offset to pointer. */
static void emit_addptr(ASMState *as, Reg r, int32_t ofs)
{
  if (ofs)
    emit_opk(as, RISCVI_ADDI, r, r, ofs);
}


#define emit_spsub(as, ofs)	emit_addptr(as, RID_SP, -(ofs))