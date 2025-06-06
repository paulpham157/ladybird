/*
 * Copyright (c) 2020, Matthew Olsson <mattco@serenityos.org>
 * Copyright (c) 2024, Tim Flynn <trflynn89@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Function.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/FunctionObject.h>
#include <LibJS/Runtime/GlobalObject.h>
#include <LibJS/Runtime/Iterator.h>
#include <LibJS/Runtime/IteratorConstructor.h>
#include <LibJS/Runtime/IteratorHelper.h>
#include <LibJS/Runtime/IteratorPrototype.h>
#include <LibJS/Runtime/ValueInlines.h>

namespace JS {

GC_DEFINE_ALLOCATOR(IteratorPrototype);

// 27.1.2 The %IteratorPrototype% Object, https://tc39.es/ecma262/#sec-%iteratorprototype%-object
IteratorPrototype::IteratorPrototype(Realm& realm)
    : PrototypeObject(realm.intrinsics().object_prototype())
{
}

void IteratorPrototype::initialize(Realm& realm)
{
    auto& vm = this->vm();
    Base::initialize(realm);

    u8 attr = Attribute::Writable | Attribute::Configurable;
    define_native_function(realm, vm.well_known_symbol_iterator(), symbol_iterator, 0, attr);
    define_native_function(realm, vm.names.drop, drop, 1, attr);
    define_native_function(realm, vm.names.every, every, 1, attr);
    define_native_function(realm, vm.names.filter, filter, 1, attr);
    define_native_function(realm, vm.names.find, find, 1, attr);
    define_native_function(realm, vm.names.flatMap, flat_map, 1, attr);
    define_native_function(realm, vm.names.forEach, for_each, 1, attr);
    define_native_function(realm, vm.names.map, map, 1, attr);
    define_native_function(realm, vm.names.reduce, reduce, 1, attr);
    define_native_function(realm, vm.names.some, some, 1, attr);
    define_native_function(realm, vm.names.take, take, 1, attr);
    define_native_function(realm, vm.names.toArray, to_array, 0, attr);

    // 27.1.4.1 Iterator.prototype.constructor, https://tc39.es/ecma262/#sec-iterator.prototype.constructor
    define_native_accessor(realm, vm.names.constructor, constructor_getter, constructor_setter, Attribute::Configurable);

    // 27.1.4.14 Iterator.prototype [ %Symbol.toStringTag% ], https://tc39.es/ecma262/#sec-iterator.prototype-%symbol.tostringtag%
    define_native_accessor(realm, vm.well_known_symbol_to_string_tag(), to_string_tag_getter, to_string_tag_setter, Attribute::Configurable);
}

// 27.1.4.1.1 get Iterator.prototype.constructor, https://tc39.es/ecma262/#sec-get-iterator.prototype.constructor
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::constructor_getter)
{
    auto& realm = *vm.current_realm();

    // 1. Return %Iterator%.
    return realm.intrinsics().iterator_constructor();
}

// 27.1.4.1.2 set Iterator.prototype.constructor, https://tc39.es/ecma262/#sec-set-iterator.prototype.constructor
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::constructor_setter)
{
    auto& realm = *vm.current_realm();

    // 1. Perform ? SetterThatIgnoresPrototypeProperties(this value, %Iterator.prototype%, "constructor", v).
    TRY(setter_that_ignores_prototype_properties(vm, vm.this_value(), realm.intrinsics().iterator_prototype(), vm.names.constructor, vm.argument(0)));

    // 2. Return undefined.
    return js_undefined();
}

// 27.1.4.2 Iterator.prototype.drop ( limit ), https://tc39.es/ecma262/#sec-iterator.prototype.drop
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::drop)
{
    auto& realm = *vm.current_realm();

    auto limit = vm.argument(0);

    // 1. Let O be the this value.
    // 2. If O is not an Object, throw a TypeError exception.
    auto object = TRY(this_object(vm));

    // 3. Let iterated be the Iterator Record { [[Iterator]]: O, [[NextMethod]]: undefined, [[Done]]: false }.
    auto iterated = realm.create<IteratorRecord>(object, js_undefined(), false);

    // 4. Let numLimit be Completion(ToNumber(limit)).
    // 5. IfAbruptCloseIterator(numLimit, iterated).
    auto numeric_limit = TRY_OR_CLOSE_ITERATOR(vm, iterated, limit.to_number(vm));

    // 6. If numLimit is NaN, then
    if (numeric_limit.is_nan()) {
        // a. Let error be ThrowCompletion(a newly created RangeError object).
        auto error = vm.throw_completion<RangeError>(ErrorType::NumberIsNaN, "limit"sv);

        // b. Return ? IteratorClose(iterated, error).
        return iterator_close(vm, iterated, error);
    }

    // 7. Let integerLimit be ! ToIntegerOrInfinity(numLimit).
    auto integer_limit = MUST(numeric_limit.to_integer_or_infinity(vm));

    // 8. If integerLimit < 0, then
    if (integer_limit < 0) {
        // a. Let error be ThrowCompletion(a newly created RangeError object).
        auto error = vm.throw_completion<RangeError>(ErrorType::NumberIsNegative, "limit"sv);

        // b. Return ? IteratorClose(iterated, error).
        return iterator_close(vm, iterated, error);
    }

    // 9. Set iterated to ? GetIteratorDirect(O).
    iterated = TRY(get_iterator_direct(vm, object));

    // 10. Let closure be a new Abstract Closure with no parameters that captures iterated and integerLimit and performs
    //     the following steps when called:
    auto closure = GC::create_function(realm.heap(), [integer_limit](VM& vm, IteratorHelper& iterator) -> ThrowCompletionOr<Value> {
        auto& iterated = iterator.underlying_iterator();

        // a. Let remaining be integerLimit.
        // b. Repeat, while remaining > 0,
        while (iterator.counter() < integer_limit) {
            // i. If remaining ≠ +∞, then
            //        1. Set remaining to remaining - 1.
            iterator.increment_counter();

            // ii. Let next be ? IteratorStep(iterated).
            IterationResultOrDone next = TRY(iterator_step(vm, iterated));

            // iii. If next is DONE, return ReturnCompletion(undefined).
            if (next.has<IterationDone>())
                return iterator.result(js_undefined());
        }

        // c. Repeat,

        // i. Let value be ? IteratorStepValue(iterated).
        auto value = TRY(iterator_step_value(vm, iterated));

        // ii. If value is DONE, return ReturnCompletion(undefined).
        if (!value.has_value())
            return iterator.result(js_undefined());

        // iii. Let completion be Completion(Yield(value)).
        // iv. IfAbruptCloseIterator(completion, iterated).
        return iterator.result(*value);
    });

    // 11. Let result be CreateIteratorFromClosure(closure, "Iterator Helper", %IteratorHelperPrototype%, « [[UnderlyingIterator]] »).
    // 12. Set result.[[UnderlyingIterator]] to iterated.
    auto result = TRY(IteratorHelper::create(realm, iterated, closure));

    // 11. Return result.
    return result;
}

// 27.1.4.3 Iterator.prototype.every ( predicate ), https://tc39.es/ecma262/#sec-iterator.prototype.every
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::every)
{
    auto& realm = *vm.current_realm();

    auto predicate = vm.argument(0);

    // 1. Let O be the this value.
    // 2. If O is not an Object, throw a TypeError exception.
    auto object = TRY(this_object(vm));

    // 3. Let iterated be the Iterator Record { [[Iterator]]: O, [[NextMethod]]: undefined, [[Done]]: false }.
    auto iterated = realm.create<IteratorRecord>(object, js_undefined(), false);

    // 4. If IsCallable(predicate) is false, then
    if (!predicate.is_function()) {
        // a. Let error be ThrowCompletion(a newly created TypeError object).
        auto error = vm.throw_completion<TypeError>(ErrorType::NotAFunction, "predicate"sv);

        // b. Return ? IteratorClose(iterated, error).
        return iterator_close(vm, iterated, error);
    }

    // 5. Set iterated to ? GetIteratorDirect(O).
    iterated = TRY(get_iterator_direct(vm, object));

    // 6. Let counter be 0.
    // 7. Repeat,
    for (size_t counter = 0;; ++counter) {
        // a. Let value be ? IteratorStepValue(iterated).
        auto value = TRY(iterator_step_value(vm, iterated));

        // b. If value is DONE, return true.
        if (!value.has_value())
            return Value { true };

        // c. Let result be Completion(Call(predicate, undefined, « value, 𝔽(counter) »)).
        // d. IfAbruptCloseIterator(result, iterated).
        auto result = TRY_OR_CLOSE_ITERATOR(vm, iterated, call(vm, predicate.as_function(), js_undefined(), *value, Value { counter }));

        // e. If ToBoolean(result) is false, return ? IteratorClose(iterated, NormalCompletion(false)).
        if (!result.to_boolean())
            return TRY(iterator_close(vm, iterated, normal_completion(Value { false })));

        // f. Set counter to counter + 1.
    }
}

// 27.1.4.4 Iterator.prototype.filter ( predicate ), https://tc39.es/ecma262/#sec-iterator.prototype.filter
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::filter)
{
    auto& realm = *vm.current_realm();

    auto predicate = vm.argument(0);

    // 1. Let O be the this value.
    // 2. If O is not an Object, throw a TypeError exception.
    auto object = TRY(this_object(vm));

    // 3. Let iterated be the Iterator Record { [[Iterator]]: O, [[NextMethod]]: undefined, [[Done]]: false }.
    auto iterated = realm.create<IteratorRecord>(object, js_undefined(), false);

    // 4. If IsCallable(predicate) is false, then
    if (!predicate.is_function()) {
        // a. Let error be ThrowCompletion(a newly created TypeError object).
        auto error = vm.throw_completion<TypeError>(ErrorType::NotAFunction, "predicate"sv);

        // b. Return ? IteratorClose(iterated, error).
        return iterator_close(vm, iterated, error);
    }

    // 5. Set iterated to ? GetIteratorDirect(O).
    iterated = TRY(get_iterator_direct(vm, object));

    // 6. Let closure be a new Abstract Closure with no parameters that captures iterated and predicate and performs the
    //    following steps when called:
    auto closure = GC::create_function(realm.heap(), [predicate = GC::Ref { predicate.as_function() }](VM& vm, IteratorHelper& iterator) -> ThrowCompletionOr<Value> {
        auto& iterated = iterator.underlying_iterator();

        // a. Let counter be 0.
        // b. Repeat,
        while (true) {
            // i. Let value be ? IteratorStepValue(iterated).
            auto value = TRY(iterator_step_value(vm, iterated));

            // ii. If value is DONE, return ReturnCompletion(undefined).
            if (!value.has_value())
                return iterator.result(js_undefined());

            // iii. Let selected be Completion(Call(predicate, undefined, « value, 𝔽(counter) »)).
            auto selected = call(vm, *predicate, js_undefined(), *value, Value { iterator.counter() });

            // iv. IfAbruptCloseIterator(selected, iterated).
            if (selected.is_error())
                return iterator.close_result(vm, selected.release_error());

            // vi. Set counter to counter + 1.
            // NOTE: We do this step early to ensure it occurs before returning.
            iterator.increment_counter();

            // v. If ToBoolean(selected) is true, then
            if (selected.value().to_boolean()) {
                // 1. Let completion be Completion(Yield(value)).
                // 2. IfAbruptCloseIterator(completion, iterated).
                return iterator.result(*value);
            }
        }
    });

    // 7. Let result be CreateIteratorFromClosure(closure, "Iterator Helper", %IteratorHelperPrototype%, « [[UnderlyingIterator]] »).
    // 8. Set result.[[UnderlyingIterator]] to iterated.
    auto result = TRY(IteratorHelper::create(realm, iterated, closure));

    // 9. Return result.
    return result;
}

// 27.1.4.5 Iterator.prototype.find ( predicate ), https://tc39.es/ecma262/#sec-iterator.prototype.find
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::find)
{
    auto& realm = *vm.current_realm();

    auto predicate = vm.argument(0);

    // 1. Let O be the this value.
    // 2. If O is not an Object, throw a TypeError exception.
    auto object = TRY(this_object(vm));

    // 3. Let iterated be the Iterator Record { [[Iterator]]: O, [[NextMethod]]: undefined, [[Done]]: false }.
    auto iterated = realm.create<IteratorRecord>(object, js_undefined(), false);

    // 4. If IsCallable(predicate) is false, then
    if (!predicate.is_function()) {
        // a. Let error be ThrowCompletion(a newly created TypeError object).
        auto error = vm.throw_completion<TypeError>(ErrorType::NotAFunction, "predicate"sv);

        // b. Return ? IteratorClose(iterated, error).
        return iterator_close(vm, iterated, error);
    }

    // 5. Set iterated to ? GetIteratorDirect(O).
    iterated = TRY(get_iterator_direct(vm, object));

    // 6. Let counter be 0.
    // 7. Repeat,
    for (size_t counter = 0;; ++counter) {
        // a. Let value be ? IteratorStepValue(iterated).
        auto value = TRY(iterator_step_value(vm, iterated));

        // b. If value is DONE, return undefined.
        if (!value.has_value())
            return js_undefined();

        // c. Let result be Completion(Call(predicate, undefined, « value, 𝔽(counter) »)).
        // d. IfAbruptCloseIterator(result, iterated).
        auto result = TRY_OR_CLOSE_ITERATOR(vm, iterated, call(vm, predicate.as_function(), js_undefined(), *value, Value { counter }));

        // e. If ToBoolean(result) is true, return ? IteratorClose(iterated, NormalCompletion(value)).
        if (result.to_boolean())
            return TRY(iterator_close(vm, iterated, normal_completion(*value)));

        // f. Set counter to counter + 1.
    }
}

class FlatMapIterator : public Cell {
    GC_CELL(FlatMapIterator, Cell);
    GC_DECLARE_ALLOCATOR(FlatMapIterator);

public:
    ThrowCompletionOr<Value> next(VM& vm, IteratorRecord& iterated, IteratorHelper& iterator, FunctionObject& mapper)
    {
        if (m_inner_iterator)
            return next_inner_iterator(vm, iterated, iterator, mapper);
        return next_outer_iterator(vm, iterated, iterator, mapper);
    }

    // NOTE: This implements step 6.b.vii.4.b of Iterator.prototype.flatMap.
    ThrowCompletionOr<Value> on_abrupt_completion(VM& vm, IteratorHelper& iterator, Completion const& completion)
    {
        VERIFY(m_inner_iterator);

        // b. If completion is an abrupt completion, then

        // i. Let backupCompletion be Completion(IteratorClose(innerIterator, completion)).
        auto backup_completion = iterator_close(vm, *m_inner_iterator, completion);

        // ii. IfAbruptCloseIterator(backupCompletion, iterated).
        if (backup_completion.is_error())
            return iterator.close_result(vm, backup_completion.release_error());

        // iii. Return ? IteratorClose(completion, iterated).
        return iterator.close_result(vm, completion);
    }

private:
    FlatMapIterator() = default;

    virtual void visit_edges(Visitor& visitor) override
    {
        Base::visit_edges(visitor);
        visitor.visit(m_inner_iterator);
    }

    ThrowCompletionOr<Value> next_outer_iterator(VM& vm, IteratorRecord& iterated, IteratorHelper& iterator, FunctionObject& mapper)
    {
        // i. Let value be ? IteratorStepValue(iterated).
        auto value = TRY(iterator_step_value(vm, iterated));

        // ii. If value is DONE, return undefined.
        if (!value.has_value())
            return iterator.result(js_undefined());

        // iii. Let mapped be Completion(Call(mapper, undefined, « value, 𝔽(counter) »)).
        auto mapped = call(vm, mapper, js_undefined(), *value, Value { iterator.counter() });

        // iv. IfAbruptCloseIterator(mapped, iterated).
        if (mapped.is_error())
            return iterator.close_result(vm, mapped.release_error());

        // v. Let innerIterator be Completion(GetIteratorFlattenable(mapped, reject-primitives)).
        auto inner_iterator = get_iterator_flattenable(vm, mapped.release_value(), PrimitiveHandling::RejectPrimitives);

        // vi. IfAbruptCloseIterator(innerIterator, iterated).
        if (inner_iterator.is_error())
            return iterator.close_result(vm, inner_iterator.release_error());

        // vii. Let innerAlive be true.
        m_inner_iterator = inner_iterator.release_value();

        // ix. Set counter to counter + 1.
        // NOTE: We do this step early to ensure it occurs before returning.
        iterator.increment_counter();

        // viii. Repeat, while innerAlive is true,
        return next_inner_iterator(vm, iterated, iterator, mapper);
    }

    ThrowCompletionOr<Value> next_inner_iterator(VM& vm, IteratorRecord& iterated, IteratorHelper& iterator, FunctionObject& mapper)
    {
        VERIFY(m_inner_iterator);

        // 1. Let innerValue be Completion(IteratorStepValue(innerIterator)).
        auto inner_value = iterator_step_value(vm, *m_inner_iterator);

        // 2. IfAbruptCloseIterator(innerValue, iterated).
        if (inner_value.is_error())
            return iterator.close_result(vm, inner_value.release_error());

        // 3. If innerValue is DONE, then
        if (!inner_value.value().has_value()) {
            // a. Set innerAlive to false.
            m_inner_iterator = nullptr;

            return next_outer_iterator(vm, iterated, iterator, mapper);
        }
        // 4. Else,
        else {
            // a. Let completion be Completion(Yield(innerValue)).
            // NOTE: Step b is implemented via on_abrupt_completion.
            return *inner_value.release_value();
        }
    }

    GC::Ptr<IteratorRecord> m_inner_iterator;
};

GC_DEFINE_ALLOCATOR(FlatMapIterator);

// 27.1.4.6 Iterator.prototype.flatMap ( mapper ), https://tc39.es/ecma262/#sec-iterator.prototype.flatmap
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::flat_map)
{
    auto& realm = *vm.current_realm();

    auto mapper = vm.argument(0);

    // 1. Let O be the this value.
    // 2. If O is not an Object, throw a TypeError exception.
    auto object = TRY(this_object(vm));

    // 3. Let iterated be the Iterator Record { [[Iterator]]: O, [[NextMethod]]: undefined, [[Done]]: false }.
    auto iterated = realm.create<IteratorRecord>(object, js_undefined(), false);

    // 4. If IsCallable(mapper) is false, then
    if (!mapper.is_function()) {
        // a. Let error be ThrowCompletion(a newly created TypeError object).
        auto error = vm.throw_completion<TypeError>(ErrorType::NotAFunction, "mapper"sv);

        // b. Return ? IteratorClose(iterated, error).
        return iterator_close(vm, iterated, error);
    }

    // 5. Set iterated to ? GetIteratorDirect(O).
    iterated = TRY(get_iterator_direct(vm, object));

    auto flat_map_iterator = realm.create<FlatMapIterator>();

    // 7. Let closure be a new Abstract Closure with no parameters that captures iterated and mapper and performs the
    //    following steps when called:
    auto closure = GC::create_function(realm.heap(), [flat_map_iterator, mapper = GC::Ref { mapper.as_function() }](VM& vm, IteratorHelper& iterator) mutable -> ThrowCompletionOr<Value> {
        auto& iterated = iterator.underlying_iterator();
        return flat_map_iterator->next(vm, iterated, iterator, *mapper);
    });

    auto abrupt_closure = GC::create_function(realm.heap(), [flat_map_iterator](VM& vm, IteratorHelper& iterator, Completion const& completion) -> ThrowCompletionOr<Value> {
        return flat_map_iterator->on_abrupt_completion(vm, iterator, completion);
    });

    // 8. Let result be CreateIteratorFromClosure(closure, "Iterator Helper", %IteratorHelperPrototype%, « [[UnderlyingIterator]] »).
    // 9. Set result.[[UnderlyingIterator]] to iterated.
    auto result = TRY(IteratorHelper::create(realm, iterated, closure, move(abrupt_closure)));

    // 9. Return result.
    return result;
}

// 27.1.4.7 Iterator.prototype.forEach ( procedure ), https://tc39.es/ecma262/#sec-iterator.prototype.foreach
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::for_each)
{
    auto& realm = *vm.current_realm();

    auto procedure = vm.argument(0);

    // 1. Let O be the this value.
    // 2. If O is not an Object, throw a TypeError exception.
    auto object = TRY(this_object(vm));

    // 3. Let iterated be the Iterator Record { [[Iterator]]: O, [[NextMethod]]: undefined, [[Done]]: false }.
    auto iterated = realm.create<IteratorRecord>(object, js_undefined(), false);

    // 4. If IsCallable(procedure) is false, then
    if (!procedure.is_function()) {
        // a. Let error be ThrowCompletion(a newly created TypeError object).
        auto error = vm.throw_completion<TypeError>(ErrorType::NotAFunction, "procedure"sv);

        // b. Return ? IteratorClose(iterated, error).
        return iterator_close(vm, iterated, error);
    }

    // 5. Set iterated to ? GetIteratorDirect(O).
    iterated = TRY(get_iterator_direct(vm, object));

    // 6. Let counter be 0.
    // 7. Repeat,
    for (size_t counter = 0;; ++counter) {
        // a. Let value be ? IteratorStepValue(iterated).
        auto value = TRY(iterator_step_value(vm, iterated));

        // b. If value is DONE, return undefined.
        if (!value.has_value())
            return js_undefined();

        // c. Let result be Completion(Call(procedure, undefined, « value, 𝔽(counter) »)).
        // d. IfAbruptCloseIterator(result, iterated).
        TRY_OR_CLOSE_ITERATOR(vm, iterated, call(vm, procedure.as_function(), js_undefined(), *value, Value { counter }));

        // e. Set counter to counter + 1.
    }
}

// 27.1.4.8 Iterator.prototype.map ( mapper ), https://tc39.es/ecma262/#sec-iterator.prototype.map
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::map)
{
    auto& realm = *vm.current_realm();

    auto mapper = vm.argument(0);

    // 1. Let O be the this value.
    // 2. If O is not an Object, throw a TypeError exception.
    auto object = TRY(this_object(vm));

    // 3. Let iterated be the Iterator Record { [[Iterator]]: O, [[NextMethod]]: undefined, [[Done]]: false }.
    auto iterated = realm.create<IteratorRecord>(object, js_undefined(), false);

    // 4. If IsCallable(mapper) is false, then
    if (!mapper.is_function()) {
        // a. Let error be ThrowCompletion(a newly created TypeError object).
        auto error = vm.throw_completion<TypeError>(ErrorType::NotAFunction, "mapper"sv);

        // b. Return ? IteratorClose(iterated, error).
        return iterator_close(vm, iterated, error);
    }

    // 5. Set iterated to ? GetIteratorDirect(O).
    iterated = TRY(get_iterator_direct(vm, object));

    // 6. Let closure be a new Abstract Closure with no parameters that captures iterated and mapper and performs the
    //    following steps when called:
    auto closure = GC::create_function(realm.heap(), [mapper = GC::Ref { mapper.as_function() }](VM& vm, IteratorHelper& iterator) -> ThrowCompletionOr<Value> {
        auto& iterated = iterator.underlying_iterator();

        // a. Let counter be 0.
        // b. Repeat,

        // i. Let value be ? IteratorStepValue(iterated).
        auto value = TRY(iterator_step_value(vm, iterated));

        // ii. If value is DONE, return undefined.
        if (!value.has_value())
            return iterator.result(js_undefined());

        // iii. Let mapped be Completion(Call(mapper, undefined, « value, 𝔽(counter) »)).
        auto mapped = call(vm, *mapper, js_undefined(), *value, Value { iterator.counter() });

        // iv. IfAbruptCloseIterator(mapped, iterated).
        if (mapped.is_error())
            return iterator.close_result(vm, mapped.release_error());

        // vii. Set counter to counter + 1.
        // NOTE: We do this step early to ensure it occurs before returning.
        iterator.increment_counter();

        // v. Let completion be Completion(Yield(mapped)).
        // vi. IfAbruptCloseIterator(completion, iterated).
        return iterator.result(mapped.release_value());
    });

    // 7. Let result be CreateIteratorFromClosure(closure, "Iterator Helper", %IteratorHelperPrototype%, « [[UnderlyingIterator]] »).
    // 8. Set result.[[UnderlyingIterator]] to iterated.
    auto result = TRY(IteratorHelper::create(realm, iterated, closure));

    // 9. Return result.
    return result;
}

// 27.1.4.9 Iterator.prototype.reduce ( reducer [ , initialValue ] ), https://tc39.es/ecma262/#sec-iterator.prototype.reduce
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::reduce)
{
    auto& realm = *vm.current_realm();

    auto reducer = vm.argument(0);

    // 1. Let O be the this value.
    // 2. If O is not an Object, throw a TypeError exception.
    auto object = TRY(this_object(vm));

    // 3. Let iterated be the Iterator Record { [[Iterator]]: O, [[NextMethod]]: undefined, [[Done]]: false }.
    auto iterated = realm.create<IteratorRecord>(object, js_undefined(), false);

    // 4. If IsCallable(reducer) is false, then
    if (!reducer.is_function()) {
        // a. Let error be ThrowCompletion(a newly created TypeError object).
        auto error = vm.throw_completion<TypeError>(ErrorType::NotAFunction, "reducer"sv);

        // b. Return ? IteratorClose(iterated, error).
        return iterator_close(vm, iterated, error);
    }

    // 5. Set iterated to ? GetIteratorDirect(O).
    iterated = TRY(get_iterator_direct(vm, object));

    Value accumulator;
    size_t counter = 0;

    // 6. If initialValue is not present, then
    if (vm.argument_count() < 2) {
        // a. Let accumulator be ? IteratorStepValue(iterated).
        auto maybe_accumulator = TRY(iterator_step_value(vm, iterated));

        // b. If accumulator is DONE, throw a TypeError exception.
        if (!maybe_accumulator.has_value())
            return vm.throw_completion<TypeError>(ErrorType::ReduceNoInitial);

        // c. Let counter be 1.
        counter = 1;

        accumulator = maybe_accumulator.release_value();
    }
    // 7. Else,
    else {
        // a. Let accumulator be initialValue.
        accumulator = vm.argument(1);

        // b. Let counter be 0.
        counter = 0;
    }

    // 8. Repeat,
    for (;; ++counter) {
        // a. Let value be ? IteratorStepValue(iterated).
        auto value = TRY(iterator_step_value(vm, iterated));

        // b. If value is DONE, return accumulator.
        if (!value.has_value())
            return accumulator;

        // c. Let result be Completion(Call(reducer, undefined, « accumulator, value, 𝔽(counter) »)).
        auto result = call(vm, reducer.as_function(), js_undefined(), accumulator, *value, Value { counter });

        // d. IfAbruptCloseIterator(result, iterated).
        if (result.is_error())
            return TRY(iterator_close(vm, iterated, result.release_error()));

        // e. Set accumulator to result.[[Value]].
        accumulator = result.release_value();

        // f. Set counter to counter + 1.
    }
}

// 27.1.4.10 Iterator.prototype.some ( predicate ), https://tc39.es/ecma262/#sec-iterator.prototype.some
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::some)
{
    auto& realm = *vm.current_realm();

    auto predicate = vm.argument(0);

    // 1. Let O be the this value.
    // 2. If O is not an Object, throw a TypeError exception.
    auto object = TRY(this_object(vm));

    // 3. Let iterated be the Iterator Record { [[Iterator]]: O, [[NextMethod]]: undefined, [[Done]]: false }.
    auto iterated = realm.create<IteratorRecord>(object, js_undefined(), false);

    // 4. If IsCallable(predicate) is false, then
    if (!predicate.is_function()) {
        // a. Let error be ThrowCompletion(a newly created TypeError object).
        auto error = vm.throw_completion<TypeError>(ErrorType::NotAFunction, "predicate"sv);

        // b. Return ? IteratorClose(iterated, error).
        return iterator_close(vm, iterated, error);
    }

    // 5. Set iterated to ? GetIteratorDirect(O).
    iterated = TRY(get_iterator_direct(vm, object));

    // 6. Let counter be 0.
    // 7. Repeat,
    for (size_t counter = 0;; ++counter) {
        // a. Let value be ? IteratorStepValue(iterated).
        auto value = TRY(iterator_step_value(vm, iterated));

        // b. If value is DONE, return false.
        if (!value.has_value())
            return Value { false };

        // c. Let result be Completion(Call(predicate, undefined, « value, 𝔽(counter) »)).
        auto result = call(vm, predicate.as_function(), js_undefined(), *value, Value { counter });

        // d. IfAbruptCloseIterator(result, iterated).
        if (result.is_error())
            return TRY(iterator_close(vm, iterated, result.release_error()));

        // e. If ToBoolean(result) is true, return ? IteratorClose(iterated, NormalCompletion(true)).
        if (result.value().to_boolean())
            return TRY(iterator_close(vm, iterated, normal_completion(Value { true })));

        // f. Set counter to counter + 1.
    }
}

// 27.1.4.11 Iterator.prototype.take ( limit ), https://tc39.es/ecma262/#sec-iterator.prototype.take
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::take)
{
    auto& realm = *vm.current_realm();

    auto limit = vm.argument(0);

    // 1. Let O be the this value.
    // 2. If O is not an Object, throw a TypeError exception.
    auto object = TRY(this_object(vm));

    // 3. Let iterated be the Iterator Record { [[Iterator]]: O, [[NextMethod]]: undefined, [[Done]]: false }.
    auto iterated = realm.create<IteratorRecord>(object, js_undefined(), false);

    // 4. Let numLimit be Completion(ToNumber(limit)).
    // 5. IfAbruptCloseIterator(numLimit, iterated).
    auto numeric_limit = TRY_OR_CLOSE_ITERATOR(vm, iterated, limit.to_number(vm));

    // 6. If numLimit is NaN, then
    if (numeric_limit.is_nan()) {
        // a. Let error be ThrowCompletion(a newly created RangeError object).
        auto error = vm.throw_completion<RangeError>(ErrorType::NumberIsNaN, "limit"sv);

        // b. Return ? IteratorClose(iterated, error).
        return iterator_close(vm, iterated, error);
    }

    // 7. Let integerLimit be ! ToIntegerOrInfinity(numLimit).
    auto integer_limit = MUST(numeric_limit.to_integer_or_infinity(vm));

    // 8. If integerLimit < 0, then
    if (integer_limit < 0) {
        // a. Let error be ThrowCompletion(a newly created RangeError object).
        auto error = vm.throw_completion<RangeError>(ErrorType::NumberIsNegative, "limit"sv);

        // b. Return ? IteratorClose(iterated, error).
        return iterator_close(vm, iterated, error);
    }

    // 9. Set iterated to ? GetIteratorDirect(O).
    iterated = TRY(get_iterator_direct(vm, object));

    // 10. Let closure be a new Abstract Closure with no parameters that captures iterated and integerLimit and performs
    //     the following steps when called:
    auto closure = GC::create_function(realm.heap(), [integer_limit](VM& vm, IteratorHelper& iterator) -> ThrowCompletionOr<Value> {
        auto& iterated = iterator.underlying_iterator();

        // a. Let remaining be integerLimit.
        // b. Repeat,

        // i. If remaining = 0, then
        if (iterator.counter() >= integer_limit) {
            // 1. Return ? IteratorClose(iterated, NormalCompletion(undefined)).
            return iterator.close_result(vm, normal_completion(js_undefined()));
        }

        // ii. If remaining ≠ +∞, then
        //     1. Set remaining to remaining - 1.
        iterator.increment_counter();

        // iii. Let value be ? IteratorStepValue(iterated).
        auto value = TRY(iterator_step_value(vm, iterated));

        // iv. If value is DONE, return ReturnCompletion(undefined)..
        if (!value.has_value())
            return iterator.result(js_undefined());

        // v. Let completion be Completion(Yield(value)).
        // vi. IfAbruptCloseIterator(completion, iterated).
        return iterator.result(*value);
    });

    // 11. Let result be CreateIteratorFromClosure(closure, "Iterator Helper", %IteratorHelperPrototype%, « [[UnderlyingIterator]] »).
    // 12. Set result.[[UnderlyingIterator]] to iterated.
    auto result = TRY(IteratorHelper::create(realm, iterated, closure));

    // 13. Return result.
    return result;
}

// 27.1.4.12 Iterator.prototype.toArray ( ), https://tc39.es/ecma262/#sec-iterator.prototype.toarray
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::to_array)
{
    auto& realm = *vm.current_realm();

    // 1. Let O be the this value.
    // 2. If O is not an Object, throw a TypeError exception.
    auto object = TRY(this_object(vm));

    // 3. Let iterated be ? GetIteratorDirect(O).
    auto iterated = TRY(get_iterator_direct(vm, object));

    // 4. Let items be a new empty List.
    GC::RootVector<Value> items(realm.heap());

    // 5. Repeat,
    while (true) {
        // a. Let value be ? IteratorStepValue(iterated).
        auto value = TRY(iterator_step_value(vm, iterated));

        // b. If value is DONE, return CreateArrayFromList(items).
        if (!value.has_value())
            return Array::create_from(realm, items);

        // c. Append value to items.
        items.append(*value);
    }
}

// 27.1.4.13 Iterator.prototype [ %Symbol.iterator% ] ( ), https://tc39.es/ecma262/#sec-iterator.prototype-%symbol.iterator%
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::symbol_iterator)
{
    // 1. Return the this value.
    return vm.this_value();
}

// 27.1.4.14.1 get Iterator.prototype [ %Symbol.toStringTag% ], https://tc39.es/ecma262/#sec-get-iterator.prototype-%symbol.tostringtag%
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::to_string_tag_getter)
{
    // 1. Return "Iterator".
    return PrimitiveString::create(vm, vm.names.Iterator.as_string());
}

// 27.1.4.14.2 set Iterator.prototype [ %Symbol.toStringTag% ], https://tc39.es/ecma262/#sec-set-iterator.prototype-%symbol.tostringtag%
JS_DEFINE_NATIVE_FUNCTION(IteratorPrototype::to_string_tag_setter)
{
    auto& realm = *vm.current_realm();

    // 1. Perform ? SetterThatIgnoresPrototypeProperties(this value, %Iterator.prototype%, %Symbol.toStringTag%, v).
    TRY(setter_that_ignores_prototype_properties(vm, vm.this_value(), realm.intrinsics().iterator_prototype(), vm.well_known_symbol_to_string_tag(), vm.argument(0)));

    // 2. Return undefined.
    return js_undefined();
}

}
