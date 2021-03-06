// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-typed-lowering.h"
#include "src/code-factory.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/isolate-inl.h"
#include "test/unittests/compiler/compiler-test-utils.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::_;
using testing::BitEq;
using testing::IsNaN;


namespace v8 {
namespace internal {
namespace compiler {

namespace {

const size_t kIndices[] = {0, 1, 42, 100, 1024};

Type* const kJSTypes[] = {Type::Undefined(), Type::Null(),   Type::Boolean(),
                          Type::Number(),    Type::String(), Type::Object()};

}  // namespace


class JSTypedLoweringTest : public TypedGraphTest {
 public:
  JSTypedLoweringTest() : TypedGraphTest(3), javascript_(zone()) {}
  ~JSTypedLoweringTest() override {}

 protected:
  Reduction Reduce(Node* node) {
    MachineOperatorBuilder machine(zone());
    SimplifiedOperatorBuilder simplified(zone());
    JSGraph jsgraph(isolate(), graph(), common(), javascript(), &simplified,
                    &machine);
    // TODO(titzer): mock the GraphReducer here for better unit testing.
    GraphReducer graph_reducer(zone(), graph());
    JSTypedLowering reducer(&graph_reducer, &jsgraph, zone());
    return reducer.Reduce(node);
  }

  Handle<JSArrayBuffer> NewArrayBuffer(void* bytes, size_t byte_length) {
    Handle<JSArrayBuffer> buffer = factory()->NewJSArrayBuffer();
    JSArrayBuffer::Setup(buffer, isolate(), true, bytes, byte_length);
    return buffer;
  }

  JSOperatorBuilder* javascript() { return &javascript_; }

 private:
  JSOperatorBuilder javascript_;
};


// -----------------------------------------------------------------------------
// JSToBoolean


TEST_F(JSTypedLoweringTest, JSToBooleanWithBoolean) {
  Node* input = Parameter(Type::Boolean(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(input, r.replacement());
}


TEST_F(JSTypedLoweringTest, JSToBooleanWithOrderedNumber) {
  Node* input = Parameter(Type::OrderedNumber(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsBooleanNot(IsNumberEqual(input, IsNumberConstant(0.0))));
}

TEST_F(JSTypedLoweringTest, JSToBooleanWithNumber) {
  Node* input = Parameter(Type::Number(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberToBoolean(input));
}

TEST_F(JSTypedLoweringTest, JSToBooleanWithDetectableReceiverOrNull) {
  Node* input = Parameter(Type::DetectableReceiverOrNull(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsBooleanNot(IsReferenceEqual(input, IsNullConstant())));
}

TEST_F(JSTypedLoweringTest, JSToBooleanWithReceiverOrNullOrUndefined) {
  Node* input = Parameter(Type::ReceiverOrNullOrUndefined(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsBooleanNot(IsObjectIsUndetectable(input)));
}

TEST_F(JSTypedLoweringTest, JSToBooleanWithString) {
  Node* input = Parameter(Type::String(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsBooleanNot(IsReferenceEqual(
                  input, IsHeapConstant(factory()->empty_string()))));
}

TEST_F(JSTypedLoweringTest, JSToBooleanWithAny) {
  Node* input = Parameter(Type::Any(), 0);
  Node* context = Parameter(Type::Any(), 1);
  Reduction r = Reduce(graph()->NewNode(
      javascript()->ToBoolean(ToBooleanHint::kAny), input, context));
  ASSERT_FALSE(r.Changed());
}


// -----------------------------------------------------------------------------
// JSToName

TEST_F(JSTypedLoweringTest, JSToNameWithString) {
  Node* const input = Parameter(Type::String(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToName(), input, context,
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(input, r.replacement());
}

TEST_F(JSTypedLoweringTest, JSToNameWithSymbol) {
  Node* const input = Parameter(Type::Symbol(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToName(), input, context,
                                        EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(input, r.replacement());
}

TEST_F(JSTypedLoweringTest, JSToNameWithAny) {
  Node* const input = Parameter(Type::Any(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToName(), input, context,
                                        EmptyFrameState(), effect, control));
  ASSERT_FALSE(r.Changed());
}

// -----------------------------------------------------------------------------
// JSToNumber

TEST_F(JSTypedLoweringTest, JSToNumberWithPlainPrimitive) {
  Node* const input = Parameter(Type::PlainPrimitive(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r =
      Reduce(graph()->NewNode(javascript()->ToNumber(), input, context,
                              EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsPlainPrimitiveToNumber(input));
}


// -----------------------------------------------------------------------------
// JSToObject


TEST_F(JSTypedLoweringTest, JSToObjectWithAny) {
  Node* const input = Parameter(Type::Any(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const frame_state = EmptyFrameState();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToObject(), input,
                                        context, frame_state, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsPhi(MachineRepresentation::kTagged, _, _, _));
}


TEST_F(JSTypedLoweringTest, JSToObjectWithReceiver) {
  Node* const input = Parameter(Type::Receiver(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const frame_state = EmptyFrameState();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToObject(), input,
                                        context, frame_state, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(input, r.replacement());
}


// -----------------------------------------------------------------------------
// JSToString


TEST_F(JSTypedLoweringTest, JSToStringWithBoolean) {
  Node* const input = Parameter(Type::Boolean(), 0);
  Node* const context = Parameter(Type::Any(), 1);
  Node* const frame_state = EmptyFrameState();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->ToString(), input,
                                        context, frame_state, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsSelect(MachineRepresentation::kTagged, input,
                       IsHeapConstant(factory()->true_string()),
                       IsHeapConstant(factory()->false_string())));
}


// -----------------------------------------------------------------------------
// JSStrictEqual

TEST_F(JSTypedLoweringTest, JSStrictEqualWithTheHole) {
  Node* const the_hole = HeapConstant(factory()->the_hole_value());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FOREACH(Type*, type, kJSTypes) {
    Node* const lhs = Parameter(type);
    Reduction r = Reduce(
        graph()->NewNode(javascript()->StrictEqual(CompareOperationHint::kAny),
                         lhs, the_hole, context, effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(), IsFalseConstant());
  }
}


TEST_F(JSTypedLoweringTest, JSStrictEqualWithUnique) {
  Node* const lhs = Parameter(Type::Unique(), 0);
  Node* const rhs = Parameter(Type::Unique(), 1);
  Node* const context = Parameter(Type::Any(), 2);
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r = Reduce(
      graph()->NewNode(javascript()->StrictEqual(CompareOperationHint::kAny),
                       lhs, rhs, context, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsReferenceEqual(lhs, rhs));
}


// -----------------------------------------------------------------------------
// JSShiftLeft

TEST_F(JSTypedLoweringTest, JSShiftLeftWithSigned32AndConstant) {
  Node* const lhs = Parameter(Type::Signed32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FORRANGE(double, rhs, 0, 31) {
    Reduction r = Reduce(graph()->NewNode(javascript()->ShiftLeft(), lhs,
                                          NumberConstant(rhs), context,
                                          EmptyFrameState(), effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsNumberShiftLeft(lhs, IsNumberConstant(BitEq(rhs))));
  }
}

TEST_F(JSTypedLoweringTest, JSShiftLeftWithSigned32AndUnsigned32) {
  Node* const lhs = Parameter(Type::Signed32());
  Node* const rhs = Parameter(Type::Unsigned32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r =
      Reduce(graph()->NewNode(javascript()->ShiftLeft(), lhs, rhs, context,
                              EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberShiftLeft(lhs, rhs));
}


// -----------------------------------------------------------------------------
// JSShiftRight


TEST_F(JSTypedLoweringTest, JSShiftRightWithSigned32AndConstant) {
  Node* const lhs = Parameter(Type::Signed32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FORRANGE(double, rhs, 0, 31) {
    Reduction r = Reduce(graph()->NewNode(javascript()->ShiftRight(), lhs,
                                          NumberConstant(rhs), context,
                                          EmptyFrameState(), effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsNumberShiftRight(lhs, IsNumberConstant(BitEq(rhs))));
  }
}


TEST_F(JSTypedLoweringTest, JSShiftRightWithSigned32AndUnsigned32) {
  Node* const lhs = Parameter(Type::Signed32());
  Node* const rhs = Parameter(Type::Unsigned32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r =
      Reduce(graph()->NewNode(javascript()->ShiftRight(), lhs, rhs, context,
                              EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberShiftRight(lhs, rhs));
}


// -----------------------------------------------------------------------------
// JSShiftRightLogical


TEST_F(JSTypedLoweringTest,
                   JSShiftRightLogicalWithUnsigned32AndConstant) {
  Node* const lhs = Parameter(Type::Unsigned32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FORRANGE(double, rhs, 0, 31) {
    Reduction r = Reduce(graph()->NewNode(javascript()->ShiftRightLogical(),
                                          lhs, NumberConstant(rhs), context,
                                          EmptyFrameState(), effect, control));
    ASSERT_TRUE(r.Changed());
    EXPECT_THAT(r.replacement(),
                IsNumberShiftRightLogical(lhs, IsNumberConstant(BitEq(rhs))));
  }
}


TEST_F(JSTypedLoweringTest, JSShiftRightLogicalWithUnsigned32AndUnsigned32) {
  Node* const lhs = Parameter(Type::Unsigned32());
  Node* const rhs = Parameter(Type::Unsigned32());
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction r =
      Reduce(graph()->NewNode(javascript()->ShiftRightLogical(), lhs, rhs,
                              context, EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsNumberShiftRightLogical(lhs, rhs));
}


// -----------------------------------------------------------------------------
// JSLoadContext


TEST_F(JSTypedLoweringTest, JSLoadContext) {
  Node* const context = Parameter(Type::Any());
  Node* const effect = graph()->start();
  static bool kBooleans[] = {false, true};
  TRACED_FOREACH(size_t, index, kIndices) {
    TRACED_FOREACH(bool, immutable, kBooleans) {
      Reduction const r1 = Reduce(graph()->NewNode(
          javascript()->LoadContext(0, index, immutable), context, effect));
      ASSERT_TRUE(r1.Changed());
      EXPECT_THAT(r1.replacement(),
                  IsLoadField(AccessBuilder::ForContextSlot(index), context,
                              effect, graph()->start()));

      Reduction const r2 = Reduce(graph()->NewNode(
          javascript()->LoadContext(1, index, immutable), context, effect));
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(r2.replacement(),
                  IsLoadField(AccessBuilder::ForContextSlot(index),
                              IsLoadField(AccessBuilder::ForContextSlot(
                                              Context::PREVIOUS_INDEX),
                                          context, effect, graph()->start()),
                              _, graph()->start()));
    }
  }
}


// -----------------------------------------------------------------------------
// JSStoreContext


TEST_F(JSTypedLoweringTest, JSStoreContext) {
  Node* const context = Parameter(Type::Any());
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  TRACED_FOREACH(size_t, index, kIndices) {
    TRACED_FOREACH(Type*, type, kJSTypes) {
      Node* const value = Parameter(type);

      Reduction const r1 =
          Reduce(graph()->NewNode(javascript()->StoreContext(0, index), value,
                                  context, effect, control));
      ASSERT_TRUE(r1.Changed());
      EXPECT_THAT(r1.replacement(),
                  IsStoreField(AccessBuilder::ForContextSlot(index), context,
                               value, effect, control));

      Reduction const r2 =
          Reduce(graph()->NewNode(javascript()->StoreContext(1, index), value,
                                  context, effect, control));
      ASSERT_TRUE(r2.Changed());
      EXPECT_THAT(r2.replacement(),
                  IsStoreField(AccessBuilder::ForContextSlot(index),
                               IsLoadField(AccessBuilder::ForContextSlot(
                                               Context::PREVIOUS_INDEX),
                                           context, effect, graph()->start()),
                               value, _, control));
    }
  }
}


// -----------------------------------------------------------------------------
// JSLoadNamed


TEST_F(JSTypedLoweringTest, JSLoadNamedStringLength) {
  VectorSlotPair feedback;
  Handle<Name> name = factory()->length_string();
  Node* const receiver = Parameter(Type::String(), 0);
  Node* const context = UndefinedConstant();
  Node* const effect = graph()->start();
  Node* const control = graph()->start();
  Reduction const r =
      Reduce(graph()->NewNode(javascript()->LoadNamed(name, feedback), receiver,
                              context, EmptyFrameState(), effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(), IsLoadField(AccessBuilder::ForStringLength(),
                                           receiver, effect, control));
}


// -----------------------------------------------------------------------------
// JSAdd


TEST_F(JSTypedLoweringTest, JSAddWithString) {
  BinaryOperationHint const hint = BinaryOperationHint::kAny;
  Node* lhs = Parameter(Type::String(), 0);
  Node* rhs = Parameter(Type::String(), 1);
  Node* context = Parameter(Type::Any(), 2);
  Node* frame_state = EmptyFrameState();
  Node* effect = graph()->start();
  Node* control = graph()->start();
  Reduction r = Reduce(graph()->NewNode(javascript()->Add(hint), lhs, rhs,
                                        context, frame_state, effect, control));
  ASSERT_TRUE(r.Changed());
  EXPECT_THAT(r.replacement(),
              IsCall(_, IsHeapConstant(
                            CodeFactory::StringAdd(
                                isolate(), STRING_ADD_CHECK_NONE, NOT_TENURED)
                                .code()),
                     lhs, rhs, context, frame_state, effect, control));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
