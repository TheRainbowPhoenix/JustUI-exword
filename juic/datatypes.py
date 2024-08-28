from dataclasses import dataclass, field
from typing import Callable, Union

# Mapping of Jui types to Python types:
#   Null           None
#   Booleans       bool
#   Integers       int
#   Floats         float
#   Strings        str
#   C/C++ refs     CXXQualid
#   List           list
#   "this"         ThisRef
#   Functions      Function, BuiltinFunction
#   Record types   RecordType
#   Records        Record

@dataclass
class CXXQualid:
    qualid: str

@dataclass
class BuiltinFunction:
    func: Callable

@dataclass
class Function:
    # Context in which the function gets evaluated
    closure: "juic.eval.Closure"
    # Expression node to evaluate when calling the function
    body: "juic.parser.Node"
    # Parameter names, must all be unique. May be empty
    params: list[str] = field(default_factory=[])
    # Name of variadic argument if one; must also be unique
    variadic: str | None = None

@dataclass
class RecordType:
    # Record name
    name: str
    # Base type for inheritance/subconstructor logic
    base: Union[None, "RecordType"]

    # TODO: Record prototypes?

# @dataclass
# class RecordCtor:
#     # Context in which the record body is evaluated
#     closure: "juic.eval.Closure"
#     # List of entries to produce, of type `REC_ATTR` or `REC_VALUE`
#     entries: list["juic.parser.Node"]
#     # Parameter names, must all be unique. May be empty
#     params: list[str] = field(default_factory=[])
#     # Name of variadic argument if one; must also be unique
#     variadic: str | None = None

#     @staticmethod
#     def makePlainRecordCtor():
#         return RecordCtor(closure=None, entries=[], params=[], variadic=None)

@dataclass
class Record:
    # A record type if it's pure, or a thunk with the base object otherwise.
    base: Union[RecordType, "juic.eval.Thunk"]
    # Standard key-value attribute pairs
    attr: dict[str, "juic.value.Thunk"]
    # Children elements
    children: list["juic.value.Thunk"]
    # TODO: Keep track of variables that are not fields, i.e. "methods"?
    # scope: dict[str, "JuiValue"]
    # TODO: Labels
    # labels: dict[str, "JuiValue"]

JuiValue = Union[None, bool, int, float, str, CXXQualid, list,
                 "juic.eval.PartialRecordSnapshot", BuiltinFunction, Function,
                 RecordType, Record]

def juiIsArith(value):
    return type(value) in [bool, int, float]

def juiIsAddable(value):
    return juiIsArith(value) or type(value) in [str, list]

def juiIsComparable(value):
    return type(value) in [None, bool, int, float, str]

def juiIsLogical(value):
    return type(value) == bool

def juiIsUnpackable(value):
    return type(value) == list

def juiIsCallable(value):
    return type(value) in [BuiltinFunction, Function]

def juiIsProjectable(value):
    return type(value) in [ThisRef, Record]

def juiIsConstructible(value):
    return type(value) in [RecordType, Function]

def juiIsRecordUpdatable(value):
    return type(value) == Record

# Generic functions on values

# String representation of a value
def juiValueString(v):
    match v:
        case None:
            return "null"
        case bool():
            return str(v).lower()
        case int() | float():
            return str(v)
        case str():
            return repr(v)
        case CXXQualid():
            return "&" + v.qualid
        case list():
            return "[" + ", ".join(juiValueString(x) for x in v) + "]"
        case BuiltinFunction():
            return str(v)
        case Function():
            p = v.params + (["..." + v.variadic] if v.variadic else [])
            s = "fun(" + ", ".join(p) + ") => " + str(v.body)
            s += " (in some closure)"
            return s
        case RecordType() as rt:
            return str(rt)
        case Record() as r:
            s = r.base.name + " {"
            s += "; ".join(x + ": " + juiValueString(y) for x, y in r.attr.items())
            s += "; ".join(juiValueString(x) for x in r.children)
            return s + "}"
            raise NotImplementedError
        case Thunk() as th:
            return str(th)
        case PartialRecordSnapshot() as prs:
            return str(prs)
        case _:
            raise NotImplementedError

# Check whether two *forced* values are equal
def juiValuesEqual(v1, v2):
    match v1, v2:
        case None, None:
            return True
        case bool(), bool():
            return v1 == v2
        case int(), int():
            return v1 == v2
        case float(), float():
            return v1 == v2
        case str(), str():
            return v1 == v2
        case CXXQualid(), CXXQualid():
            return v1.qualid == v2.qualid
        case list() as l1, list() as l2:
            return len(l1) == len(l2) and \
                all(juiValuesEqual(v1, v2) for v1, v2 in zip(l1, l2))
        case BuiltinFunction(), BuiltinFunction():
            return id(v1) == id(v2)
        case Function(), Function():
            raise Exception("cannot compare functions")
        case RecordType(), RecordType():
            return id(v1) == id(v2)
        case Record() as r1, Record() as r2:
            if not juiValuesEqual(r1.base, r2.base):
                return False
            if r1.attr.keys() != r2.attr.keys():
                return False
            if any(not juiValuesEqual(r1.attr[k], r2.attr[k]) for k in r1.attr):
                return False
            if len(r1.children) != len(r2.children):
                return False
            if any(not juiValuesEqual(v1, v2)
                   for v1, v2 in zip(r1.children, r2.children)):
                return False
            return True
        case Thunk() as th1, Thunk() as th2:
            assert th1.evaluated and not th1.invalid
            assert th2.evaluated and not th2.invalid
            return juiValuesEqual(th1.result, th2.result)
        case PartialRecordSnapshot(), PartialRecordSnapshot():
            raise Exception("cannot compare PRSs")
        case _, _:
            raise Exception(f"invalid types for comparison: {type(v1)}, {type(v2)}")
