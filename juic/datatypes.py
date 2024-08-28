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
    # TODO: Record prototypes?

    @staticmethod
    def makePlainRecordType():
        return RecordType()

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
