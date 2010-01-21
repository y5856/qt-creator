/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#include "qmljsinterpreter.h"
#include <QtCore/QDebug>

using namespace QmlJS::Interpreter;

////////////////////////////////////////////////////////////////////////////////
// ValueVisitor
////////////////////////////////////////////////////////////////////////////////
ValueVisitor::ValueVisitor() {}
ValueVisitor::~ValueVisitor() {}
void ValueVisitor::visit(const NullValue *) {}
void ValueVisitor::visit(const UndefinedValue *) {}
void ValueVisitor::visit(const NumberValue *) {}
void ValueVisitor::visit(const BooleanValue *) {}
void ValueVisitor::visit(const StringValue *) {}
void ValueVisitor::visit(const ObjectValue *) {}
void ValueVisitor::visit(const FunctionValue *) {}


////////////////////////////////////////////////////////////////////////////////
// Value
////////////////////////////////////////////////////////////////////////////////
Value::Value() {}
Value::~Value() {}
const NullValue *Value::asNullValue() const { return 0; }
const UndefinedValue *Value::asUndefinedValue() const { return 0; }
const NumberValue *Value::asNumberValue() const { return 0; }
const BooleanValue *Value::asBooleanValue() const { return 0; }
const StringValue *Value::asStringValue() const { return 0; }
const ObjectValue *Value::asObjectValue() const { return 0; }
const FunctionValue *Value::asFunctionValue() const { return 0; }

////////////////////////////////////////////////////////////////////////////////
// Environment
////////////////////////////////////////////////////////////////////////////////
Environment::Environment() {}
Environment::~Environment() {}
const Environment *Environment::parent() const { return 0; }

const Value *Environment::lookup(const QString &name) const
{
    if (const Value *member = lookupMember(name))
        return member;

    else if (const Environment *p = parent())
        return p->lookup(name);

    else
        return 0;
}

const Value *Environment::lookupMember(const QString &name) const {
    Q_UNUSED(name);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Values
////////////////////////////////////////////////////////////////////////////////
const NullValue *NullValue::asNullValue() const { return this; }
void NullValue::accept(ValueVisitor *visitor) const { visitor->visit(this); }

const UndefinedValue *UndefinedValue::asUndefinedValue() const { return this; }
void UndefinedValue::accept(ValueVisitor *visitor) const { visitor->visit(this); }

const NumberValue *NumberValue::asNumberValue() const { return this; }
void NumberValue::accept(ValueVisitor *visitor) const { visitor->visit(this); }

const BooleanValue *BooleanValue::asBooleanValue() const { return this; }
void BooleanValue::accept(ValueVisitor *visitor) const { visitor->visit(this); }

const StringValue *StringValue::asStringValue() const { return this; }
void StringValue::accept(ValueVisitor *visitor) const { visitor->visit(this); }

ObjectValue::ObjectValue(Engine *engine): _engine(engine), _prototype(0), _scope(0) {}
Engine *ObjectValue::engine() const { return _engine; }
QString ObjectValue::className() const { return _className; }
void ObjectValue::setClassName(const QString &className) { _className = className; }
const ObjectValue *ObjectValue::prototype() const { return _prototype; }
const ObjectValue *ObjectValue::scope() const { return _scope; }
void ObjectValue::setScope(const ObjectValue *scope) { _scope = scope; }
ObjectValue::MemberIterator ObjectValue::firstMember() const { return _members.begin(); }
ObjectValue::MemberIterator ObjectValue::lastMember() const { return _members.end(); }
void ObjectValue::setProperty(const QString &name, const Value *value) { _members[name] = value; }
void ObjectValue::removeProperty(const QString &name) { _members.remove(name); }
const ObjectValue *ObjectValue::asObjectValue() const { return this; }
void ObjectValue::accept(ValueVisitor *visitor) const { visitor->visit(this); }

const Value *ObjectValue::property(const QString &name) const {
    if (name == QLatin1String("__proto__"))
        return _prototype;

    return lookupMember(name);
}

void ObjectValue::setPrototype(const ObjectValue *prototype)
{
    QSet<const ObjectValue *> processed;

    if (! prototype || isValidPrototype(prototype, &processed))
        _prototype = prototype;
    else
        qWarning() << "**** invalid prototype:";
}

bool ObjectValue::isValidPrototype(const ObjectValue *proto, QSet<const ObjectValue *> *processed) const
{
    const int previousSize = processed->size();
    processed->insert(this);

    if (previousSize != processed->size()) {
        if (this == proto)
            return false;

        if (prototype() && ! prototype()->isValidPrototype(proto, processed))
            return false;

        return true;
    }

    return false;
}

const Value *ObjectValue::member(const QString &name) const
{
    MemberIterator it = _members.find(name);

    if (it != _members.end())
        return it.value();
    else
        return 0;
}

const Environment *ObjectValue::parent() const
{
    return _scope;
}

const Value *ObjectValue::lookupMember(const QString &name) const
{
    if (const Value *m = member(name))
        return m;

    if (_prototype) {
        if (const Value *m = _prototype->lookup(name))
            return m;
    }

    return 0;
}

FunctionValue::FunctionValue(Engine *engine): ObjectValue(engine) {}
int FunctionValue::argumentCount() const { return 0; }
const FunctionValue *FunctionValue::asFunctionValue() const { return this; }
void FunctionValue::accept(ValueVisitor *visitor) const { visitor->visit(this); }


Function::Function(Engine *engine): FunctionValue(engine), _returnValue(0) { setClassName("Function"); }
void Function::addArgument(const Value *argument) { _arguments.push_back(argument); }
Function::ArgumentIterator Function::firstArgument() const { return _arguments.begin(); }
Function::ArgumentIterator Function::lastArgument() const { return _arguments.end(); }
const Value *Function::returnValue() const { return _returnValue; }
void Function::setReturnValue(const Value *returnValue) { _returnValue = returnValue; }
int Function::argumentCount() const { return _arguments.size(); }
const Value *Function::argument(int index) const { return _arguments.at(index); }

const Value *Function::property(const QString &name) const
{
    if (name == "length")
        return engine()->numberValue();

    return FunctionValue::property(name);
}

const Value *Function::call(const Value *thisValue, const QList<const Value *> &) const
{
    return thisValue; // ### FIXME it should return undefined
}

////////////////////////////////////////////////////////////////////////////////
// typing environment
////////////////////////////////////////////////////////////////////////////////
ConvertToNumber::ConvertToNumber(Engine *engine): _engine(engine), _result(0) {}

const Value *ConvertToNumber::operator()(const Value *value) {
    const Value *previousValue = switchResult(0);

    if (value)
        value->accept(this);

    return switchResult(previousValue);
}

const Value *ConvertToNumber::switchResult(const Value *value) {
    const Value *previousResult = _result;
    _result = value;
    return previousResult;
}

ConvertToString::ConvertToString(Engine *engine): _engine(engine), _result(0) {}

const Value *ConvertToString::operator()(const Value *value) {
    const Value *previousValue = switchResult(0);

    if (value)
        value->accept(this);

    return switchResult(previousValue);
}

const Value *ConvertToString::switchResult(const Value *value) {
    const Value *previousResult = _result;
    _result = value;
    return previousResult;
}

ConvertToObject::ConvertToObject(Engine *engine): _engine(engine), _result(0) {}

const Value *ConvertToObject::operator()(const Value *value) {
    const Value *previousValue = switchResult(0);

    if (value)
        value->accept(this);

    return switchResult(previousValue);
}

const Value *ConvertToObject::switchResult(const Value *value) {
    const Value *previousResult = _result;
    _result = value;
    return previousResult;
}


QString TypeId::operator()(const Value *value)
{
    _result = QLatin1String("unknown");

    if (value)
        value->accept(this);

    return _result;
}

void TypeId::visit(const NullValue *) { _result = QLatin1String("null"); }
void TypeId::visit(const UndefinedValue *) { _result = QLatin1String("undefined"); }
void TypeId::visit(const NumberValue *) { _result = QLatin1String("number"); }
void TypeId::visit(const BooleanValue *) { _result = QLatin1String("boolean"); }
void TypeId::visit(const StringValue *) { _result = QLatin1String("string"); }

void TypeId::visit(const ObjectValue *object) {
    _result = object->className();

    if (_result.isEmpty())
        _result = QLatin1String("object");
}

void TypeId::visit(const FunctionValue *object) {
    _result = object->className();

    if (_result.isEmpty())
        _result = QLatin1String("Function");
}

////////////////////////////////////////////////////////////////////////////////
// constructors
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// constructors
////////////////////////////////////////////////////////////////////////////////
class ObjectCtor: public Function
{
public:
    ObjectCtor(Engine *engine);

    virtual const Value *call(const Value *thisValue, const QList<const Value *> &actuals) const;
};

class FunctionCtor: public Function
{
public:
    FunctionCtor(Engine *engine);

    virtual const Value *call(const Value *thisValue, const QList<const Value *> &actuals) const;
};

class ArrayCtor: public Function
{
public:
    ArrayCtor(Engine *engine);

    virtual const Value *call(const Value *thisValue, const QList<const Value *> &actuals) const;
};

class StringCtor: public Function
{
public:
    StringCtor(Engine *engine);

    virtual const Value *call(const Value *thisValue, const QList<const Value *> &actuals) const;
};

class BooleanCtor: public Function
{
public:
    BooleanCtor(Engine *engine);

    virtual const Value *call(const Value *thisValue, const QList<const Value *> &actuals) const;
};

class NumberCtor: public Function
{
public:
    NumberCtor(Engine *engine);

    virtual const Value *call(const Value *thisValue, const QList<const Value *> &actuals) const;
};

class DateCtor: public Function
{
public:
    DateCtor(Engine *engine);

    virtual const Value *call(const Value *thisValue, const QList<const Value *> &actuals) const;
};

class RegExpCtor: public Function
{
public:
    RegExpCtor(Engine *engine);

    virtual const Value *call(const Value *thisValue, const QList<const Value *> &actuals) const;
};

ObjectCtor::ObjectCtor(Engine *engine): Function(engine) {}
FunctionCtor::FunctionCtor(Engine *engine): Function(engine) {}
ArrayCtor::ArrayCtor(Engine *engine): Function(engine) {}
StringCtor::StringCtor(Engine *engine): Function(engine) {}
BooleanCtor::BooleanCtor(Engine *engine): Function(engine) {}
NumberCtor::NumberCtor(Engine *engine): Function(engine) {}
DateCtor::DateCtor(Engine *engine): Function(engine) {}
RegExpCtor::RegExpCtor(Engine *engine): Function(engine) {}

Engine::Engine()
    : _objectPrototype(0),
      _functionPrototype(0),
      _numberPrototype(0),
      _booleanPrototype(0),
      _stringPrototype(0),
      _arrayPrototype(0),
      _datePrototype(0),
      _regexpPrototype(0),
      _objectCtor(0),
      _functionCtor(0),
      _arrayCtor(0),
      _stringCtor(0),
      _booleanCtor(0),
      _numberCtor(0),
      _dateCtor(0),
      _regexpCtor(0),
      _globalObject(0),
      _mathObject(0),
      _convertToNumber(this),
      _convertToString(this),
      _convertToObject(this)
{

    initializePrototypes();
}

Engine::~Engine() {
    QList<ObjectValue *>::iterator it = _objects.begin();

    for (; it != _objects.end(); ++it)
        delete *it;
}

const NullValue *Engine::nullValue() const { return &_nullValue; }
const UndefinedValue *Engine::undefinedValue() const { return &_undefinedValue; }
const NumberValue *Engine::numberValue() const { return &_numberValue; }
const BooleanValue *Engine::booleanValue() const { return &_booleanValue; }
const StringValue *Engine::stringValue() const { return &_stringValue; }

const Value *Engine::newArrayValue() { return call(arrayCtor()); }

ObjectValue *Engine::newObject()
{
    return newObject(_objectPrototype);
}

ObjectValue *Engine::newObject(const ObjectValue *prototype)
{
    ObjectValue *object = new ObjectValue(this);
    object->setPrototype(prototype);
    _objects.push_back(object);
    return object;
}

Function *Engine::newFunction() {
    Function *function = new Function(this);
    function->setPrototype(functionPrototype());
    _objects.push_back(function);
    return function;
}

ObjectValue *Engine::globalObject() const { return _globalObject; }
ObjectValue *Engine::objectPrototype() const { return _objectPrototype; }
ObjectValue *Engine::functionPrototype() const { return _functionPrototype; }
ObjectValue *Engine::numberPrototype() const { return _numberPrototype; }
ObjectValue *Engine::booleanPrototype() const { return _booleanPrototype; }
ObjectValue *Engine::stringPrototype() const { return _stringPrototype; }
ObjectValue *Engine::arrayPrototype() const { return _arrayPrototype; }
ObjectValue *Engine::datePrototype() const { return _datePrototype; }
ObjectValue *Engine::regexpPrototype() const { return _regexpPrototype; }
const FunctionValue *Engine::objectCtor() const { return _objectCtor; }
const FunctionValue *Engine::functionCtor() const { return _functionCtor; }
const FunctionValue *Engine::arrayCtor() const { return _arrayCtor; }
const FunctionValue *Engine::stringCtor() const { return _stringCtor; }
const FunctionValue *Engine::booleanCtor() const { return _booleanCtor; }
const FunctionValue *Engine::numberCtor() const { return _numberCtor; }
const FunctionValue *Engine::dateCtor() const { return _dateCtor; }
const FunctionValue *Engine::regexpCtor() const { return _regexpCtor; }
const ObjectValue *Engine::mathObject() const { return _mathObject; }
const Value *Engine::convertToBool(const Value *value) { return _convertToNumber(value); } // ### implement convert to bool
const Value *Engine::convertToNumber(const Value *value) { return _convertToNumber(value); }
const Value *Engine::convertToString(const Value *value) { return _convertToString(value); }
const Value *Engine::convertToObject(const Value *value) { return _convertToObject(value); }
QString Engine::typeId(const Value *value) { return _typeId(value); }

const Value *Engine::call(const FunctionValue *function, const QList<const Value *> &actuals)
{
    return call(function, globalObject(), actuals);
}

const Value *Engine::call(const FunctionValue *function, const ObjectValue *thisValue, const QList<const Value *> &actuals)
{
    return function->call(thisValue, actuals);
}

void Engine::addFunction(ObjectValue *object, const QString &name, const Value *result, int argumentCount)
{
    Function *function = newFunction();
    function->setReturnValue(result);
    for (int i = 0; i < argumentCount; ++i)
        function->addArgument(undefinedValue()); // ### introduce unknownValue
    object->setProperty(name, function);
}

void Engine::addFunction(ObjectValue *object, const QString &name, int argumentCount)
{
    Function *function = newFunction();
    for (int i = 0; i < argumentCount; ++i)
        function->addArgument(undefinedValue()); // ### introduce unknownValue
    object->setProperty(name, function);
}

void Engine::initializePrototypes()
{
    _objectPrototype   = newObject(/*prototype = */ 0);
    _functionPrototype = newObject(_objectPrototype);
    _numberPrototype   = newObject(_objectPrototype);
    _booleanPrototype  = newObject(_objectPrototype);
    _stringPrototype   = newObject(_objectPrototype);
    _arrayPrototype    = newObject(_objectPrototype);
    _datePrototype     = newObject(_objectPrototype);
    _regexpPrototype   = newObject(_objectPrototype);

    // set up the Global object
    _globalObject = newObject();
    _globalObject->setClassName("Global");

    // set up the default Object prototype
    _objectCtor = new ObjectCtor(this);
    _objectCtor->setPrototype(_functionPrototype);
    _objects.push_back(_objectCtor);
    _objectCtor->setProperty("prototype", _objectPrototype);

    _functionCtor = new FunctionCtor(this);
    _functionCtor->setPrototype(_functionPrototype);
    _objects.push_back(_functionCtor);
    _functionCtor->setProperty("prototype", _functionPrototype);

    _arrayCtor = new ArrayCtor(this);
    _arrayCtor->setPrototype(_functionPrototype);
    _objects.push_back(_arrayCtor);
    _arrayCtor->setProperty("prototype", _arrayPrototype);

    _stringCtor = new StringCtor(this);
    _stringCtor->setPrototype(_functionPrototype);
    _objects.push_back(_stringCtor);
    _stringCtor->setProperty("prototype", _stringPrototype);

    _booleanCtor = new BooleanCtor(this);
    _booleanCtor->setPrototype(_functionPrototype);
    _objects.push_back(_booleanCtor);
    _booleanCtor->setProperty("prototype", _booleanPrototype);

    _numberCtor = new NumberCtor(this);
    _numberCtor->setPrototype(_functionPrototype);
    _objects.push_back(_numberCtor);
    _numberCtor->setProperty("prototype", _numberPrototype);

    _dateCtor = new DateCtor(this);
    _dateCtor->setPrototype(_functionPrototype);
    _objects.push_back(_dateCtor);
    _dateCtor->setProperty("prototype", _datePrototype);

    _regexpCtor = new RegExpCtor(this);
    _regexpCtor->setPrototype(_functionPrototype);
    _objects.push_back(_regexpCtor);
    _regexpCtor->setProperty("prototype", _regexpPrototype);

    addFunction(_objectCtor, "getPrototypeOf", 1);
    addFunction(_objectCtor, "getOwnPropertyDescriptor", 2);
    addFunction(_objectCtor, "getOwnPropertyNames", newArrayValue(), 1);
    addFunction(_objectCtor, "create", 1);
    addFunction(_objectCtor, "defineProperty", 3);
    addFunction(_objectCtor, "defineProperties", 2);
    addFunction(_objectCtor, "seal", 1);
    addFunction(_objectCtor, "freeze", 1);
    addFunction(_objectCtor, "preventExtensions", 1);
    addFunction(_objectCtor, "isSealed", booleanValue(), 1);
    addFunction(_objectCtor, "isFrozen", booleanValue(), 1);
    addFunction(_objectCtor, "isExtensible", booleanValue(), 1);
    addFunction(_objectCtor, "keys", newArrayValue(), 1);

    addFunction(_objectPrototype, "toString", stringValue(), 0);
    addFunction(_objectPrototype, "toLocaleString", stringValue(), 0);
    addFunction(_objectPrototype, "valueOf", 0); // ### FIXME it should return thisValue
    addFunction(_objectPrototype, "hasOwnProperty", booleanValue(), 1);
    addFunction(_objectPrototype, "isPrototypeOf", booleanValue(), 1);
    addFunction(_objectPrototype, "propertyIsEnumerable", booleanValue(), 1);

    // set up the default Function prototype
    _functionPrototype->setProperty("constructor", _functionCtor);
    addFunction(_functionPrototype, "toString", stringValue(), 0);
    addFunction(_functionPrototype, "apply", 2);
    addFunction(_functionPrototype, "call", 1);
    addFunction(_functionPrototype, "bind", 1);

    // set up the default Array prototype
    addFunction(_arrayCtor, "isArray", booleanValue(), 1);

    _arrayPrototype->setProperty("constructor", _arrayCtor);
    addFunction(_arrayPrototype, "toString", stringValue(), 0);
    addFunction(_arrayPrototype, "toLocalString", stringValue(), 0);
    addFunction(_arrayPrototype, "concat", 0);
    addFunction(_arrayPrototype, "join", 1);
    addFunction(_arrayPrototype, "pop", 0);
    addFunction(_arrayPrototype, "push", 0);
    addFunction(_arrayPrototype, "reverse", 0);
    addFunction(_arrayPrototype, "shift", 0);
    addFunction(_arrayPrototype, "slice", 2);
    addFunction(_arrayPrototype, "sort", 1);
    addFunction(_arrayPrototype, "splice", 2);
    addFunction(_arrayPrototype, "unshift", 0);
    addFunction(_arrayPrototype, "indexOf", numberValue(), 1);
    addFunction(_arrayPrototype, "lastIndexOf", numberValue(), 1);
    addFunction(_arrayPrototype, "every", 1);
    addFunction(_arrayPrototype, "some", 1);
    addFunction(_arrayPrototype, "forEach", 1);
    addFunction(_arrayPrototype, "map", 1);
    addFunction(_arrayPrototype, "filter", 1);
    addFunction(_arrayPrototype, "reduce", 1);
    addFunction(_arrayPrototype, "reduceRight", 1);

    // set up the default String prototype
    addFunction(_stringCtor, "fromCharCode", stringValue(), 0);

    _stringPrototype->setProperty("constructor", _stringCtor);
    addFunction(_stringPrototype, "toString", stringValue(), 0);
    addFunction(_stringPrototype, "valueOf", stringValue(), 0);
    addFunction(_stringPrototype, "charAt", stringValue(), 1);
    addFunction(_stringPrototype, "charCodeAt", stringValue(), 1);
    addFunction(_stringPrototype, "concat", stringValue(), 0);
    addFunction(_stringPrototype, "indexOf", numberValue(), 2);
    addFunction(_stringPrototype, "lastIndexOf", numberValue(), 2);
    addFunction(_stringPrototype, "localeCompare", booleanValue(), 1);
    addFunction(_stringPrototype, "match", newArrayValue(), 1);
    addFunction(_stringPrototype, "replace", stringValue(), 2);
    addFunction(_stringPrototype, "search", numberValue(), 1);
    addFunction(_stringPrototype, "slice", stringValue(), 2);
    addFunction(_stringPrototype, "split", newArrayValue(), 2);
    addFunction(_stringPrototype, "substring", stringValue(), 2);
    addFunction(_stringPrototype, "toLowerCase", stringValue(), 0);
    addFunction(_stringPrototype, "toLocaleLowerCase", stringValue(), 0);
    addFunction(_stringPrototype, "toUpperCase", stringValue(), 0);
    addFunction(_stringPrototype, "toLocaleUpperCase", stringValue(), 0);
    addFunction(_stringPrototype, "trim", stringValue(), 0);

    // set up the default Boolean prototype
    addFunction(_booleanCtor, "fromCharCode", 0);

    _booleanPrototype->setProperty("constructor", _booleanCtor);
    addFunction(_booleanPrototype, "toString", stringValue(), 0);
    addFunction(_booleanPrototype, "valueOf", booleanValue(), 0);

    // set up the default Number prototype
    _numberCtor->setProperty("MAX_VALUE", numberValue());
    _numberCtor->setProperty("MIN_VALUE", numberValue());
    _numberCtor->setProperty("NaN", numberValue());
    _numberCtor->setProperty("NEGATIVE_INFINITY", numberValue());
    _numberCtor->setProperty("POSITIVE_INFINITY", numberValue());

    addFunction(_numberCtor, "fromCharCode", 0);

    _numberPrototype->setProperty("constructor", _numberCtor);
    addFunction(_numberPrototype, "toString", stringValue(), 0);
    addFunction(_numberPrototype, "toLocaleString", stringValue(), 0);
    addFunction(_numberPrototype, "valueOf", numberValue(), 0);
    addFunction(_numberPrototype, "toFixed", numberValue(), 1);
    addFunction(_numberPrototype, "toExponential", numberValue(), 1);
    addFunction(_numberPrototype, "toPrecision", numberValue(), 1);

    // set up the Math object
    _mathObject = newObject();
    _mathObject->setProperty("E", numberValue());
    _mathObject->setProperty("LN10", numberValue());
    _mathObject->setProperty("LN2", numberValue());
    _mathObject->setProperty("LOG2E", numberValue());
    _mathObject->setProperty("LOG10E", numberValue());
    _mathObject->setProperty("PI", numberValue());
    _mathObject->setProperty("SQRT1_2", numberValue());
    _mathObject->setProperty("SQRT2", numberValue());

    addFunction(_mathObject, "abs", numberValue(), 1);
    addFunction(_mathObject, "acos", numberValue(), 1);
    addFunction(_mathObject, "asin", numberValue(), 1);
    addFunction(_mathObject, "atan", numberValue(), 1);
    addFunction(_mathObject, "atan2", numberValue(), 2);
    addFunction(_mathObject, "ceil", numberValue(), 1);
    addFunction(_mathObject, "cos", numberValue(), 1);
    addFunction(_mathObject, "exp", numberValue(), 1);
    addFunction(_mathObject, "floor", numberValue(), 1);
    addFunction(_mathObject, "log", numberValue(), 1);
    addFunction(_mathObject, "max", numberValue(), 1);
    addFunction(_mathObject, "min", numberValue(), 1);
    addFunction(_mathObject, "pow", numberValue(), 2);
    addFunction(_mathObject, "random", numberValue(), 1);
    addFunction(_mathObject, "round", numberValue(), 1);
    addFunction(_mathObject, "sin", numberValue(), 1);
    addFunction(_mathObject, "sqrt", numberValue(), 1);
    addFunction(_mathObject, "tan", numberValue(), 1);

    // set up the default Boolean prototype
    addFunction(_dateCtor, "parse", numberValue(), 1);
    addFunction(_dateCtor, "now", numberValue(), 0);

    _datePrototype->setProperty("constructor", _dateCtor);
    addFunction(_datePrototype, "toString", stringValue(), 0);
    addFunction(_datePrototype, "toDateString", stringValue(), 0);
    addFunction(_datePrototype, "toTimeString", stringValue(), 0);
    addFunction(_datePrototype, "toLocaleString", stringValue(), 0);
    addFunction(_datePrototype, "toLocaleDateString", stringValue(), 0);
    addFunction(_datePrototype, "toLocaleTimeString", stringValue(), 0);
    addFunction(_datePrototype, "valueOf", numberValue(), 0);
    addFunction(_datePrototype, "getTime", numberValue(), 0);
    addFunction(_datePrototype, "getFullYear", numberValue(), 0);
    addFunction(_datePrototype, "getUTCFullYear", numberValue(), 0);
    addFunction(_datePrototype, "getMonth", numberValue(), 0);
    addFunction(_datePrototype, "getUTCMonth", numberValue(), 0);
    addFunction(_datePrototype, "getDate", numberValue(), 0);
    addFunction(_datePrototype, "getUTCDate", numberValue(), 0);
    addFunction(_datePrototype, "getHours", numberValue(), 0);
    addFunction(_datePrototype, "getUTCHours", numberValue(), 0);
    addFunction(_datePrototype, "getMinutes", numberValue(), 0);
    addFunction(_datePrototype, "getUTCMinutes", numberValue(), 0);
    addFunction(_datePrototype, "getSeconds", numberValue(), 0);
    addFunction(_datePrototype, "getUTCSeconds", numberValue(), 0);
    addFunction(_datePrototype, "getMilliseconds", numberValue(), 0);
    addFunction(_datePrototype, "getUTCMilliseconds", numberValue(), 0);
    addFunction(_datePrototype, "getTimezoneOffset", numberValue(), 0);
    addFunction(_datePrototype, "setTime", 1);
    addFunction(_datePrototype, "setMilliseconds", 1);
    addFunction(_datePrototype, "setUTCMilliseconds", 1);
    addFunction(_datePrototype, "setSeconds", 1);
    addFunction(_datePrototype, "setUTCSeconds", 1);
    addFunction(_datePrototype, "setMinutes", 1);
    addFunction(_datePrototype, "setUTCMinutes", 1);
    addFunction(_datePrototype, "setHours", 1);
    addFunction(_datePrototype, "setUTCHours", 1);
    addFunction(_datePrototype, "setDate", 1);
    addFunction(_datePrototype, "setUTCDate", 1);
    addFunction(_datePrototype, "setMonth", 1);
    addFunction(_datePrototype, "setUTCMonth", 1);
    addFunction(_datePrototype, "setFullYear", 1);
    addFunction(_datePrototype, "setUTCFullYear", 1);
    addFunction(_datePrototype, "toUTCString", stringValue(), 0);
    addFunction(_datePrototype, "toISOString", stringValue(), 0);
    addFunction(_datePrototype, "toJSON", stringValue(), 1);

    // set up the default Boolean prototype
    _regexpPrototype->setProperty("constructor", _regexpCtor);
    addFunction(_regexpPrototype, "exec", newArrayValue(), 1);
    addFunction(_regexpPrototype, "test", booleanValue(), 1);
    addFunction(_regexpPrototype, "toString", stringValue(), 0);

    // fill the Global object
    _globalObject->setProperty("Math", _mathObject);
    _globalObject->setProperty("Object", objectCtor());
    _globalObject->setProperty("Function", functionCtor());
    _globalObject->setProperty("Array", arrayCtor());
    _globalObject->setProperty("String", stringCtor());
    _globalObject->setProperty("Boolean", booleanCtor());
    _globalObject->setProperty("Number", numberCtor());
    _globalObject->setProperty("Date", dateCtor());
    _globalObject->setProperty("RegExp", regexpCtor());
}

const Value *FunctionValue::call(const Value *thisValue, const QList<const Value *> & /*actuals*/) const
{
    return thisValue; // ### FIXME: it should return undefined
}

const Value *FunctionValue::argument(int /*index*/) const
{
    return engine()->undefinedValue();
}

const Value *FunctionValue::returnValue() const
{
    return engine()->undefinedValue();
}

const Value *ObjectCtor::call(const Value *, const QList<const Value *> &) const
{
    ObjectValue *object = engine()->newObject();
    object->setClassName("Object");
    object->setPrototype(engine()->objectPrototype());
    object->setProperty("length", engine()->numberValue());
    return object;
}

const Value *FunctionCtor::call(const Value *, const QList<const Value *> &) const
{
    ObjectValue *object = engine()->newObject();
    object->setClassName("Function");
    object->setPrototype(engine()->functionPrototype());
    object->setProperty("length", engine()->numberValue());
    return object;
}

const Value *ArrayCtor::call(const Value *thisValue, const QList<const Value *> &) const
{
    ObjectValue *object = const_cast<ObjectValue *>(value_cast<const ObjectValue *>(thisValue));
    if (! object || object == engine()->globalObject())
        object = engine()->newObject();

    object->setClassName("Array");
    object->setPrototype(engine()->arrayPrototype());
    object->setProperty("length", engine()->numberValue());
    return object;
}

const Value *StringCtor::call(const Value *thisValue, const QList<const Value *> &) const
{
    ObjectValue *object = const_cast<ObjectValue *>(value_cast<const ObjectValue *>(thisValue));
    if (! object || object == engine()->globalObject())
        object = engine()->newObject();

    object->setClassName("String");
    object->setPrototype(engine()->stringPrototype());
    object->setProperty("length", engine()->numberValue());
    return object;
}

const Value *BooleanCtor::call(const Value *, const QList<const Value *> &) const
{
    ObjectValue *object = engine()->newObject();
    object->setClassName("Boolean");
    object->setPrototype(engine()->booleanPrototype());
    return object;
}

const Value *NumberCtor::call(const Value *, const QList<const Value *> &) const
{
    ObjectValue *object = engine()->newObject();
    object->setClassName("Number");
    object->setPrototype(engine()->numberPrototype());
    return object;
}

const Value *DateCtor::call(const Value *, const QList<const Value *> &) const
{
    ObjectValue *object = engine()->newObject();
    object->setClassName("Date");
    object->setPrototype(engine()->datePrototype());
    return object;
}

const Value *RegExpCtor::call(const Value *, const QList<const Value *> &) const
{
    ObjectValue *object = engine()->newObject();
    object->setClassName("RegExp");
    object->setPrototype(engine()->regexpPrototype());
    object->setProperty("source", engine()->stringValue());
    object->setProperty("global", engine()->booleanValue());
    object->setProperty("ignoreCase", engine()->booleanValue());
    object->setProperty("multiline", engine()->booleanValue());
    object->setProperty("lastIndex", engine()->numberValue());
    return object;
}

////////////////////////////////////////////////////////////////////////////////
// convert to number
////////////////////////////////////////////////////////////////////////////////
void ConvertToNumber::visit(const NullValue *)
{
    _result = _engine->numberValue();
}

void ConvertToNumber::visit(const UndefinedValue *)
{
    _result = _engine->numberValue();
}

void ConvertToNumber::visit(const NumberValue *value)
{
    _result = value;
}

void ConvertToNumber::visit(const BooleanValue *)
{
    _result = _engine->numberValue();
}

void ConvertToNumber::visit(const StringValue *)
{
    _result = _engine->numberValue();
}

void ConvertToNumber::visit(const ObjectValue *object)
{
    if (const FunctionValue *valueOfMember = value_cast<const FunctionValue *>(object->lookup("valueOf"))) {
        _result = value_cast<const NumberValue *>(valueOfMember->call(object)); // ### invoke convert-to-number?
    }
}

void ConvertToNumber::visit(const FunctionValue *object)
{
    if (const FunctionValue *valueOfMember = value_cast<const FunctionValue *>(object->lookup("valueOf"))) {
        _result = value_cast<const NumberValue *>(valueOfMember->call(object)); // ### invoke convert-to-number?
    }
}

////////////////////////////////////////////////////////////////////////////////
// convert to string
////////////////////////////////////////////////////////////////////////////////
void ConvertToString::visit(const NullValue *)
{
    _result = _engine->stringValue();
}

void ConvertToString::visit(const UndefinedValue *)
{
    _result = _engine->stringValue();
}

void ConvertToString::visit(const NumberValue *)
{
    _result = _engine->stringValue();
}

void ConvertToString::visit(const BooleanValue *)
{
    _result = _engine->stringValue();
}

void ConvertToString::visit(const StringValue *value)
{
    _result = value;
}

void ConvertToString::visit(const ObjectValue *object)
{
    if (const FunctionValue *toStringMember = value_cast<const FunctionValue *>(object->lookup("toString"))) {
        _result = value_cast<const StringValue *>(toStringMember->call(object)); // ### invoke convert-to-string?
    }
}

void ConvertToString::visit(const FunctionValue *object)
{
    if (const FunctionValue *toStringMember = value_cast<const FunctionValue *>(object->lookup("toString"))) {
        _result = value_cast<const StringValue *>(toStringMember->call(object)); // ### invoke convert-to-string?
    }
}

////////////////////////////////////////////////////////////////////////////////
// convert to object
////////////////////////////////////////////////////////////////////////////////
void ConvertToObject::visit(const NullValue *value)
{
    _result = value;
}

void ConvertToObject::visit(const UndefinedValue *)
{
    _result = _engine->nullValue();
}

void ConvertToObject::visit(const NumberValue *)
{
    _result = _engine->call(_engine->numberCtor());
}

void ConvertToObject::visit(const BooleanValue *)
{
    _result = _engine->call(_engine->booleanCtor());
}

void ConvertToObject::visit(const StringValue *)
{
    _result = _engine->call(_engine->stringCtor());
}

void ConvertToObject::visit(const ObjectValue *object)
{
    _result = object;
}

void ConvertToObject::visit(const FunctionValue *object)
{
    _result = object;
}

