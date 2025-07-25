/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibJS/Runtime/NativeFunction.h>

namespace JS {

class JS_API DataViewConstructor final : public NativeFunction {
    JS_OBJECT(DataViewConstructor, NativeFunction);
    GC_DECLARE_ALLOCATOR(DataViewConstructor);

public:
    virtual void initialize(Realm&) override;
    virtual ~DataViewConstructor() override = default;

    virtual ThrowCompletionOr<Value> call() override;
    virtual ThrowCompletionOr<GC::Ref<Object>> construct(FunctionObject&) override;

private:
    explicit DataViewConstructor(Realm&);

    virtual bool has_constructor() const override { return true; }
};

}
