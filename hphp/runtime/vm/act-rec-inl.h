/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-present Facebook, Inc. (http://www.facebook.com)  |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#include "hphp/util/compilation-flags.h"

#include "hphp/runtime/vm/func.h"

namespace HPHP {

///////////////////////////////////////////////////////////////////////////////

inline const Func* ActRec::func() const {
  return m_func;
}

inline const Unit* ActRec::unit() const {
  func()->validate();
  return func()->unit();
}

inline ActRec* ActRec::sfp() const {
  if (UNLIKELY(((uintptr_t)m_sfp - s_stackLimit) < s_stackSize)) {
    return nullptr;
  }
  return m_sfp;
}

///////////////////////////////////////////////////////////////////////////////

inline int32_t ActRec::numArgs() const {
  return m_numArgsAndFlags & kNumArgsMask;
}

inline ActRec::Flags ActRec::flags() const {
  return static_cast<Flags>(m_numArgsAndFlags & kFlagsMask);
}

inline bool ActRec::localsDecRefd() const {
  return flags() & LocalsDecRefd;
}

inline bool ActRec::resumed() const {
  return (flags() & kExecutionModeMask) == InResumed;
}

inline bool ActRec::isAsyncEagerReturn() const {
  return (flags() & kExecutionModeMask) == AsyncEagerRet;
}

inline uint32_t ActRec::encodeNumArgsAndFlags(uint32_t numArgs, Flags flags) {
  assertx((numArgs & kFlagsMask) == 0);
  assertx((uint32_t{flags} & kNumArgsMask) == 0);
  return numArgs | flags;
}

inline void ActRec::initNumArgs(uint32_t numArgs) {
  m_numArgsAndFlags = encodeNumArgsAndFlags(numArgs, Flags::None);
}

inline void ActRec::setNumArgs(uint32_t numArgs) {
  m_numArgsAndFlags = encodeNumArgsAndFlags(numArgs, flags());
}

inline void ActRec::setLocalsDecRefd() {
  m_numArgsAndFlags |= LocalsDecRefd;
}

inline void ActRec::setResumed() {
  assertx((flags() & ~AsyncEagerRet) == Flags::None);
  m_numArgsAndFlags = encodeNumArgsAndFlags(
    numArgs(),
    static_cast<Flags>(InResumed)
  );
}

inline void ActRec::setAsyncEagerReturn() {
  assertx(flags() == Flags::None);
  m_numArgsAndFlags = encodeNumArgsAndFlags(
    numArgs(),
    static_cast<Flags>(AsyncEagerRet)
  );
}

///////////////////////////////////////////////////////////////////////////////

inline void* ActRec::encodeThis(ObjectData* obj) {
  return obj;
}

inline void* ActRec::encodeClass(const Class* cls) {
  return cls
    ? (void*)(reinterpret_cast<uintptr_t>(cls) | kHasClassBit)
    : nullptr;
}

inline bool ActRec::checkThis(void* p) {
  assertx(p);
  return !(reinterpret_cast<uintptr_t>(p) & kHasClassBit);
}

inline bool ActRec::checkThisOrNull(void* p) {
  return !(reinterpret_cast<uintptr_t>(p) & kHasClassBit);
}

inline ObjectData* ActRec::decodeThis(void* p) {
  return checkThisOrNull(p) ? (ObjectData*)p : nullptr;
}

inline Class* ActRec::decodeClass(void* p) {
  return checkThisOrNull(p) ? nullptr :
    (Class*)(reinterpret_cast<uintptr_t>(p) - kHasClassBit);
}

inline void ActRec::setThisOrClass(void* objOrCls) {
  assertx(m_func->implCls());
  setThisOrClassAllowNull(objOrCls);
}

inline void ActRec::setThisOrClassAllowNull(void* objOrCls) {
  m_thisUnsafe = (ObjectData*)objOrCls;
}

inline bool ActRec::hasThis() const {
  assertx(m_func->implCls());
  assertx(reinterpret_cast<uintptr_t>(m_thisUnsafe) != kTrashedThisSlot);
  return checkThis(m_thisUnsafe);
}

inline bool ActRec::hasClass() const {
  assertx(m_func->implCls());
  return !checkThis(m_thisUnsafe);
}

inline void* ActRec::getThisOrClass() const {
  assertx(m_func->implCls());
  return m_thisUnsafe;
}

inline ObjectData* ActRec::getThisUnsafe() const {
  return m_thisUnsafe;
}

inline ObjectData* ActRec::getThis() const {
  assertx(hasThis());
  return m_thisUnsafe;
}

inline Class* ActRec::getClass() const {
  assertx(hasClass());
  return reinterpret_cast<Class*>(
    reinterpret_cast<uintptr_t>(m_clsUnsafe) - kHasClassBit);
}

inline void ActRec::setThis(ObjectData* val) {
  assertx(m_func->implCls() && !m_func->isStaticInPrologue());
  m_thisUnsafe = val;
}

inline void ActRec::setClass(Class* val) {
  assertx(val && m_func->implCls() && (m_func->isStaticInPrologue()));
  m_clsUnsafe = reinterpret_cast<Class*>(
    reinterpret_cast<uintptr_t>(val) | kHasClassBit);
}

inline void ActRec::trashThis() {
  if (debug) m_thisUnsafe = reinterpret_cast<ObjectData*>(kTrashedThisSlot);
}

/////////////////////////////////////////////////////////////////////////////

inline void ActRec::trashVarEnv() {
  if (debug) setVarEnv(reinterpret_cast<VarEnv*>(kTrashedVarEnvSlot));
}

inline bool ActRec::checkVarEnv() const {
  assertx(m_varEnv != reinterpret_cast<VarEnv*>(kTrashedVarEnvSlot));
  return true;
}

inline bool ActRec::hasVarEnv() const {
  assertx(checkVarEnv());
  return m_varEnv;
}

inline VarEnv* ActRec::getVarEnv() const {
  assertx(hasVarEnv());
  return m_varEnv;
}

inline void ActRec::setVarEnv(VarEnv* val) {
  m_varEnv = val;
}

///////////////////////////////////////////////////////////////////////////////

inline RxLevel ActRec::rxMinLevel() const {
  if (func()->isRxConditional()) return RxLevel::None;
  return func()->rxLevel();
}

///////////////////////////////////////////////////////////////////////////////

}
