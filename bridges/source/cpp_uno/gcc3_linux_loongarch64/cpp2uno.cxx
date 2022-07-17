/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <sal/config.h>

#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <dlfcn.h>

#include <com/sun/star/uno/XInterface.hpp>
#include <com/sun/star/uno/genfunc.hxx>
#include <sal/alloca.h>
#include <sal/types.h>
#include <typelib/typeclass.h>
#include <typelib/typedescription.h>
#include <typelib/typedescription.hxx>

#include <bridge.hxx>
#include <cppinterfaceproxy.hxx>
#include <types.hxx>
#include <vtablefactory.hxx>

#include "abi.hxx"

extern "C" void vtableSlotCall_();

namespace {

void call(
    bridges::cpp_uno::shared::CppInterfaceProxy * proxy,
    css::uno::TypeDescription const & description,
    typelib_TypeDescriptionReference * returnType, sal_Int32 count,
    typelib_MethodParameter * parameters, unsigned long * gpr,
    unsigned long * fpr, unsigned long * stack, void * indirectRet)
{
    typelib_TypeDescription * rtd = 0;
    if (returnType != 0) {
        TYPELIB_DANGER_GET(&rtd, returnType);
    }
    abi_loongarch64::ReturnKind retKind = rtd == 0
        ? abi_loongarch64::RETURN_KIND_REG : abi_loongarch64::getReturnKind(rtd);
    bool retConv = rtd != 0
        && bridges::cpp_uno::shared::relatesToInterfaceType(rtd);
    void * retin = retKind == abi_loongarch64::RETURN_KIND_INDIRECT && !retConv
        ? indirectRet : rtd == 0 ? 0 : alloca(rtd->nSize);
    void ** args = static_cast< void ** >(alloca(count * sizeof (void *)));
    void ** cppArgs = static_cast< void ** >(alloca(count * sizeof (void *)));
    typelib_TypeDescription ** argtds = static_cast<typelib_TypeDescription **>(
        alloca(count * sizeof (typelib_TypeDescription *)));
    sal_Int32 ngpr = 1;
    sal_Int32 nfpr = 0;
    sal_Int32 sp = 0;
    for (sal_Int32 i = 0; i != count; ++i) {
        if (!parameters[i].bOut
            && bridges::cpp_uno::shared::isSimpleType(parameters[i].pTypeRef))
        {
            switch (parameters[i].pTypeRef->eTypeClass) {
            case typelib_TypeClass_BOOLEAN:
            case typelib_TypeClass_BYTE:
            case typelib_TypeClass_SHORT:
            case typelib_TypeClass_UNSIGNED_SHORT:
            case typelib_TypeClass_LONG:
            case typelib_TypeClass_UNSIGNED_LONG:
            case typelib_TypeClass_HYPER:
            case typelib_TypeClass_UNSIGNED_HYPER:
            case typelib_TypeClass_CHAR:
            case typelib_TypeClass_ENUM:
                args[i] = ngpr == 8 ? stack + sp++ : gpr + ngpr++;
                break;
            case typelib_TypeClass_FLOAT:
            case typelib_TypeClass_DOUBLE:
                args[i] = nfpr == 8 ? stack + sp++ : fpr + nfpr++;
                break;
            default:
                assert(false);
            }
            argtds[i] = 0;
        } else {
            cppArgs[i] = reinterpret_cast<void *>(
                ngpr == 8 ? stack[sp++] : gpr[ngpr++]);
            typelib_TypeDescription * ptd = 0;
            TYPELIB_DANGER_GET(&ptd, parameters[i].pTypeRef);
            if (!parameters[i].bIn) {
                args[i] = alloca(ptd->nSize);
                argtds[i] = ptd;
            } else if (bridges::cpp_uno::shared::relatesToInterfaceType(ptd)) {
                args[i] = alloca(ptd->nSize);
                uno_copyAndConvertData(
                    args[i], cppArgs[i], ptd, proxy->getBridge()->getCpp2Uno());
                argtds[i] = ptd;
            } else {
                args[i] = cppArgs[i];
                argtds[i] = 0;
                TYPELIB_DANGER_RELEASE(ptd);
            }
        }
    }
    uno_Any exc;
    uno_Any * pexc = &exc;
    proxy->getUnoI()->pDispatcher(
        proxy->getUnoI(), description.get(), retin, args, &pexc);
    if (pexc != 0) {
        for (sal_Int32 i = 0; i != count; ++i) {
            if (argtds[i] != 0) {
                if (parameters[i].bIn) {
                    uno_destructData(args[i], argtds[i], 0);
                }
                TYPELIB_DANGER_RELEASE(argtds[i]);
            }
        }
        if (rtd != 0) {
            TYPELIB_DANGER_RELEASE(rtd);
        }
        abi_loongarch64::raiseException(&exc, proxy->getBridge()->getUno2Cpp());
    }
    for (sal_Int32 i = 0; i != count; ++i) {
        if (argtds[i] != 0) {
            if (parameters[i].bOut) {
                uno_destructData(
                    cppArgs[i], argtds[i],
                    reinterpret_cast<uno_ReleaseFunc>(css::uno::cpp_release));
                uno_copyAndConvertData(
                    cppArgs[i], args[i], argtds[i],
                    proxy->getBridge()->getUno2Cpp());
            }
            uno_destructData(args[i], argtds[i], 0);
            TYPELIB_DANGER_RELEASE(argtds[i]);
        }
    }
    void * retout = 0; // avoid false -Werror=maybe-uninitialized
    switch (retKind) {
    case abi_loongarch64::RETURN_KIND_REG:
        switch (rtd == 0 ? typelib_TypeClass_VOID : rtd->eTypeClass) {
        case typelib_TypeClass_VOID:
            break;
        case typelib_TypeClass_BOOLEAN:
        case typelib_TypeClass_BYTE:
        case typelib_TypeClass_SHORT:
        case typelib_TypeClass_UNSIGNED_SHORT:
        case typelib_TypeClass_LONG:
        case typelib_TypeClass_UNSIGNED_LONG:
        case typelib_TypeClass_HYPER:
        case typelib_TypeClass_UNSIGNED_HYPER:
        case typelib_TypeClass_CHAR:
        case typelib_TypeClass_ENUM:
            std::memcpy(gpr, retin, rtd->nSize);
            assert(!retConv);
            break;
        case typelib_TypeClass_FLOAT:
        case typelib_TypeClass_DOUBLE:
            std::memcpy(fpr, retin, rtd->nSize);
            assert(!retConv);
            break;
        case typelib_TypeClass_STRUCT:
            if (retConv) {
                retout = gpr;
            } else {
                std::memcpy(gpr, retin, rtd->nSize);
            }
            break;
        default:
            assert(false);
        }
        break;
    case abi_loongarch64::RETURN_KIND_HFA_FLOAT:
        assert(rtd != 0);
        switch (rtd->nSize) {
        case 16:
            std::memcpy(fpr + 3, static_cast<char *>(retin) + 12, 4);
            // fall through
        case 12:
            std::memcpy(fpr + 2, static_cast<char *>(retin) + 8, 4);
            // fall through
        case 8:
            std::memcpy(fpr + 1, static_cast<char *>(retin) + 4, 4);
            // fall through
        case 4:
            std::memcpy(fpr, retin, 4);
            break;
        default:
            assert(false);
        }
        assert(!retConv);
        break;
    case abi_loongarch64::RETURN_KIND_HFA_DOUBLE:
        assert(rtd != 0);
        std::memcpy(fpr, retin, rtd->nSize);
        assert(!retConv);
        break;
    case abi_loongarch64::RETURN_KIND_INDIRECT:
        retout = indirectRet;
        break;
    }
    if (retConv) {
        uno_copyAndConvertData(
            retout, retin, rtd, proxy->getBridge()->getUno2Cpp());
        uno_destructData(retin, rtd, 0);
    }
    if (rtd != 0) {
        TYPELIB_DANGER_RELEASE(rtd);
    }
}

extern "C" void vtableCall(
    sal_Int32 functionIndex, sal_Int32 vtableOffset,
    unsigned long * gpr, unsigned long * fpr, unsigned long  * stack,
    void * indirectRet)
{
    bridges::cpp_uno::shared::CppInterfaceProxy * proxy
        = bridges::cpp_uno::shared::CppInterfaceProxy::castInterfaceToProxy(
            reinterpret_cast<char *>(gpr[0]) - vtableOffset);
    typelib_InterfaceTypeDescription * type = proxy->getTypeDescr();
    assert(functionIndex < type->nMapFunctionIndexToMemberIndex);
    sal_Int32 pos = type->pMapFunctionIndexToMemberIndex[functionIndex];
    css::uno::TypeDescription desc(type->ppAllMembers[pos]);
    switch (desc.get()->eTypeClass) {
    case typelib_TypeClass_INTERFACE_ATTRIBUTE:
        if (type->pMapMemberIndexToFunctionIndex[pos] == functionIndex) {
            // Getter:
            call(
                proxy, desc,
                reinterpret_cast<typelib_InterfaceAttributeTypeDescription *>(
                    desc.get())->pAttributeTypeRef,
                0, 0, gpr, fpr, stack, indirectRet);
        } else {
            // Setter:
            typelib_MethodParameter param = {
                0,
                reinterpret_cast<typelib_InterfaceAttributeTypeDescription *>(
                    desc.get())->pAttributeTypeRef,
                true, false };
            call(proxy, desc, 0, 1, &param, gpr, fpr, stack, indirectRet);
        }
        break;
    case typelib_TypeClass_INTERFACE_METHOD:
        switch (functionIndex) {
        case 1:
            proxy->acquireProxy();
            break;
        case 2:
            proxy->releaseProxy();
            break;
        case 0:
            {
                typelib_TypeDescription * td = 0;
                TYPELIB_DANGER_GET(
                    &td,
                    (reinterpret_cast<css::uno::Type *>(gpr[1])
                     ->getTypeLibType()));
                if (td != 0 && td->eTypeClass == typelib_TypeClass_INTERFACE) {
                    css::uno::XInterface * ifc = 0;
                    proxy->getBridge()->getCppEnv()->getRegisteredInterface(
                        proxy->getBridge()->getCppEnv(),
                        reinterpret_cast<void **>(&ifc), proxy->getOid().pData,
                        reinterpret_cast<typelib_InterfaceTypeDescription *>(
                            td));
                    if (ifc != 0) {
                        uno_any_construct(
                            reinterpret_cast<uno_Any *>(indirectRet), &ifc, td,
                            reinterpret_cast<uno_AcquireFunc>(
                                css::uno::cpp_acquire));
                        ifc->release();
                        TYPELIB_DANGER_RELEASE(td);
                        break;
                    }
                    TYPELIB_DANGER_RELEASE(td);
                }
            }
            // fall through
        default:
            call(
                proxy, desc,
                reinterpret_cast<typelib_InterfaceMethodTypeDescription *>(
                    desc.get())->pReturnTypeRef,
                reinterpret_cast<typelib_InterfaceMethodTypeDescription *>(
                    desc.get())->nParams,
                reinterpret_cast<typelib_InterfaceMethodTypeDescription *>(
                    desc.get())->pParams,
                gpr, fpr, stack, indirectRet);
        }
        break;
    default:
        assert(false);
    }
}

struct loongarch64_va_list {
    void * stack;
    void * gr_top;
    void * vr_top;
    int gr_offs;
    int vr_offs;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile-register-var"
extern "C" void vtableSlotCall(
    unsigned long gpr0, unsigned long gpr1, unsigned long gpr2,
    unsigned long gpr3, unsigned long gpr4, unsigned long gpr5,
    unsigned long gpr6, unsigned long gpr7, double fpr0, double fpr1,
    double fpr2, double fpr3, double fpr4, double fpr5, double fpr6,
    double fpr7, ...)
{
    register void * volatile indirectRet asm ("$t0");
    register sal_Int32 volatile functionIndex asm ("$t6");
    register sal_Int32 volatile vtableOffset asm ("$t7");
    va_list ap;
    va_start(ap, fpr7);
    assert(sizeof (va_list) == sizeof (loongarch64_va_list));
    unsigned long gpr[8];
    gpr[0] = gpr0;
    gpr[1] = gpr1;
    gpr[2] = gpr2;
    gpr[3] = gpr3;
    gpr[4] = gpr4;
    gpr[5] = gpr5;
    gpr[6] = gpr6;
    gpr[7] = gpr7;
    double fpr[8];
    fpr[0] = fpr0;
    fpr[1] = fpr1;
    fpr[2] = fpr2;
    fpr[3] = fpr3;
    fpr[4] = fpr4;
    fpr[5] = fpr5;
    fpr[6] = fpr6;
    fpr[7] = fpr7;
    vtableCall(
        functionIndex, vtableOffset, gpr,
        reinterpret_cast<unsigned long *>(fpr),
        static_cast<unsigned long *>(
            reinterpret_cast<loongarch64_va_list *>(&ap)->stack),
        indirectRet);
    asm volatile(
        "ld.d $r4, %[gpr_], 0\n\t"
        "ld.d $r5, %[gpr_], 8\n\t"
        "fld.d $f0, %[fpr_],0\n\t"
        "fld.d $f1, %[fpr_],8\n\t"
        "fld.d $f2, %[fpr_],16\n\t"
        "fld.d $f3, %[fpr_],24\n\t"
        :: [gpr_]"r" (gpr), [fpr_]"r" (fpr)
        : "$r4", "$r5", "$f0", "$f1", "$f2", "$f3");
    va_end(ap);
}
#pragma GCC diagnostic pop

std::size_t const codeSnippetSize = 0x24;

unsigned char * generateCodeSnippet(
    unsigned char * code, sal_Int32 functionIndex, sal_Int32 vtableOffset)
{
    // lu12i.w $t6, <high functionIndex>
    reinterpret_cast<unsigned int *>(code)[0] = 0x14000012
        | (((functionIndex >> 12) & 0x000fffff) << 5);
    // ori $t6, $t6, <low functionIndex>
    reinterpret_cast<unsigned int *>(code)[1] = 0x03800252
        | ((functionIndex & 0x00000fff) << 10);
    // lu12i.w $t7, <low vtableOffset>
    reinterpret_cast<unsigned int *>(code)[2] = 0x14000013
        | (((vtableOffset >> 12) & 0x000fffff) << 5);
    // ori $t7, $t7, <high vtableOffset>
    reinterpret_cast<unsigned int *>(code)[3] = 0x03800273
        | ((vtableOffset & 0x00000fff) << 10);

    // load vtableSlotCall to $t8
    // lu12i.w $t8, <high vtableSlotCall>
    reinterpret_cast<unsigned int *>(code)[4] = 0x14000014
        | (((((unsigned long)vtableSlotCall) >> 12) & 0x000fffff) << 5);
    // ori $t8, $t8, <low vtableSlotCall>
    reinterpret_cast<unsigned int *>(code)[5] = 0x03800294
        | ((((unsigned long)vtableSlotCall) & 0x00000fff) << 10);
    // lu32i.d $t8, <higher vtableSlotCall>
    reinterpret_cast<unsigned int *>(code)[6] = 0x16000014
        | (((((unsigned long)vtableSlotCall) >> 32) & 0x000fffff) << 5);
    // lu52i.d $t8, $t8, <top vtableSlotCall>
    reinterpret_cast<unsigned int *>(code)[7] = 0x03000294
        | (((((unsigned long)vtableSlotCall) >> 52) & 0x00000fff) << 10);
    // jr $t8
    reinterpret_cast<unsigned int *>(code)[8] = 0x4c000280;
    return code + codeSnippetSize;
}

}

struct bridges::cpp_uno::shared::VtableFactory::Slot { void * fn; };

bridges::cpp_uno::shared::VtableFactory::Slot *
bridges::cpp_uno::shared::VtableFactory::mapBlockToVtable(void * block) {
    return static_cast<Slot *>(block) + 2;
}

std::size_t bridges::cpp_uno::shared::VtableFactory::getBlockSize(
    sal_Int32 slotCount)
{
    return (slotCount + 2) * sizeof (Slot) + slotCount * codeSnippetSize;
}

bridges::cpp_uno::shared::VtableFactory::Slot *
bridges::cpp_uno::shared::VtableFactory::initializeBlock(
    void * block, sal_Int32 slotCount, sal_Int32,
    typelib_InterfaceTypeDescription *)
{
    Slot * slots = mapBlockToVtable(block);
    slots[-2].fn = 0;
    slots[-1].fn = 0;
    return slots + slotCount;
}

unsigned char * bridges::cpp_uno::shared::VtableFactory::addLocalFunctions(
    Slot ** slots, unsigned char * code, sal_PtrDiff writetoexecdiff,
    typelib_InterfaceTypeDescription const * type, sal_Int32 functionOffset,
    sal_Int32 functionCount, sal_Int32 vtableOffset)
{
    (*slots) -= functionCount;
    Slot * s = *slots;
    for (sal_Int32 i = 0; i != type->nMembers; ++i) {
        typelib_TypeDescription * td = 0;
        TYPELIB_DANGER_GET(&td, type->ppMembers[i]);
        assert(td != 0);
        switch (td->eTypeClass) {
        case typelib_TypeClass_INTERFACE_ATTRIBUTE:
            {
                typelib_InterfaceAttributeTypeDescription * atd
                    = reinterpret_cast<
                        typelib_InterfaceAttributeTypeDescription *>(td);
                // Getter:
                (s++)->fn = code + writetoexecdiff;
                code = generateCodeSnippet(
                    code, functionOffset++, vtableOffset);
                // Setter:
                if (!atd->bReadOnly) {
                    (s++)->fn = code + writetoexecdiff;
                    code = generateCodeSnippet(
                        code, functionOffset++, vtableOffset);
                }
                break;
            }
        case typelib_TypeClass_INTERFACE_METHOD:
            (s++)->fn = code + writetoexecdiff;
            code = generateCodeSnippet(code, functionOffset++, vtableOffset);
            break;
        default:
            assert(false);
        }
        TYPELIB_DANGER_RELEASE(td);
    }
    return code;
}

void bridges::cpp_uno::shared::VtableFactory::flushCode(
    unsigned char const *, unsigned char const *)
{
    // Consistency between Icache and Dcache is maintained by hardware on
    // LoongArch systems, and data barriers are certainly involved during
    // thread migration. Hence, ibar on the current core should do the trick
    // of ensuring the newly written code is visible before being invoked.
    asm volatile ("ibar 0" : : : "memory");
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
