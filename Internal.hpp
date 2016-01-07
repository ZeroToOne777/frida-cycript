/* Cycript - The Truly Universal Scripting Language
 * Copyright (C) 2009-2016  Jay Freeman (saurik)
*/

/* GNU Affero General Public License, Version 3 {{{ */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.

 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/
/* }}} */

#ifndef CYCRIPT_INTERNAL_HPP
#define CYCRIPT_INTERNAL_HPP

#include <sig/parse.hpp>
#include <sig/ffi_type.hpp>

#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSValueRef.h>

#include "JavaScript.hpp"
#include "Pooling.hpp"
#include "Utility.hpp"

JSGlobalContextRef CYGetJSContext(JSContextRef context);
sig::Type *Structor_(CYPool &pool, sig::Aggregate *aggregate);

extern JSClassRef Functor_;

template <typename Internal_>
struct CYPrivate :
    CYData
{
    static JSClassRef Class_;

    _finline JSValueRef GetPrototype(JSContextRef context) const {
        return NULL;
    }

    template <typename... Args_>
    _finline static JSClassRef GetClass(Args_ &&... args) {
        return Class_;
    }

    template <typename... Args_>
    static JSObjectRef Make(JSContextRef context, Args_ &&... args) {
        Internal_ *internal(new Internal_(cy::Forward<Args_>(args)...));
        JSObjectRef object(JSObjectMake(context, Internal_::GetClass(cy::Forward<Args_>(args)...), internal));
        if (JSValueRef prototype = internal->GetPrototype(context))
            CYSetPrototype(context, object, prototype);
        return object;
    }

    static Internal_ *Get(JSContextRef context, JSObjectRef object) {
        _assert(JSValueIsObjectOfClass(context, object, Class_));
        return static_cast<Internal_ *>(JSObjectGetPrivate(object));
    }
};

struct Type_privateData :
    CYPrivate<Type_privateData>
{
    ffi_type *ffi_;
    sig::Type *type_;

    Type_privateData(const char *type) :
        ffi_(NULL)
    {
        sig::Signature signature;
        sig::Parse(*pool_, &signature, type, &Structor_);
        type_ = signature.elements[0].type;
    }

    Type_privateData(const sig::Type &type, ffi_type *ffi = NULL) :
        type_(type.Copy(*pool_))
    {

        if (ffi == NULL)
            ffi_ = NULL;
        else {
            ffi_ = new(*pool_) ffi_type;
            sig::Copy(*pool_, *ffi_, *ffi);
        }
    }

    ffi_type *GetFFI() {
        if (ffi_ == NULL) {
            sig::Element element;
            element.name = NULL;
            element.type = type_;
            element.offset = 0;

            sig::Signature signature;
            signature.elements = &element;
            signature.count = 1;

            ffi_cif cif;
            sig::sig_ffi_cif(*pool_, false, signature, &cif);

            ffi_ = new(*pool_) ffi_type;
            *ffi_ = *cif.rtype;
        }

        return ffi_;
    }
};

template <typename Internal_>
JSClassRef CYPrivate<Internal_>::Class_;

struct CYProtect {
  private:
    JSGlobalContextRef context_;
    JSObjectRef object_;

  public:
    CYProtect(JSContextRef context, JSObjectRef object) :
        context_(CYGetJSContext(context)),
        object_(object)
    {
        //XXX:JSGlobalContextRetain(context_);
        if (object_ != NULL)
            JSValueProtect(context_, object_);
    }

    ~CYProtect() {
        if (object_ != NULL)
            JSValueUnprotect(context_, object_);
        //XXX:JSGlobalContextRelease(context_);
    }

    operator bool() const {
        return object_ != NULL;
    }

    operator JSContextRef() const {
        return context_;
    }

    operator JSObjectRef() const {
        return object_;
    }
};

namespace cy {
struct Functor :
    CYPrivate<Functor>
{
  private:
    void set() {
        sig::sig_ffi_cif(*pool_, variadic_ ? signature_.count : 0, signature_, &cif_);
    }

  public:
    void (*value_)();
    bool variadic_;
    sig::Signature signature_;
    ffi_cif cif_;

    Functor(void (*value)(), bool variadic, const sig::Signature &signature) :
        value_(value),
        variadic_(variadic)
    {
        sig::Copy(*pool_, signature_, signature);
        set();
    }

    Functor(void (*value)(), const char *encoding) :
        value_(value),
        variadic_(false)
    {
        sig::Parse(*pool_, &signature_, encoding, &Structor_);
        set();
    }

    static JSStaticFunction const * const StaticFunctions;
    static JSStaticValue const * const StaticValues;
}; }

struct Closure_privateData :
    cy::Functor
{
    CYProtect function_;
    JSValueRef (*adapter_)(JSContextRef, size_t, JSValueRef[], JSObjectRef);

    Closure_privateData(JSContextRef context, JSObjectRef function, JSValueRef (*adapter)(JSContextRef, size_t, JSValueRef[], JSObjectRef), const sig::Signature &signature) :
        cy::Functor(NULL, false, signature),
        function_(context, function),
        adapter_(adapter)
    {
    }
};

Closure_privateData *CYMakeFunctor_(JSContextRef context, JSObjectRef function, const sig::Signature &signature, JSValueRef (*adapter)(JSContextRef, size_t, JSValueRef[], JSObjectRef));
void CYExecuteClosure(ffi_cif *cif, void *result, void **arguments, void *arg, JSValueRef (*adapter)(JSContextRef, size_t, JSValueRef[], JSObjectRef));

#endif/*CYCRIPT_INTERNAL_HPP*/
