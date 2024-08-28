from juic.datatypes import *
from juic.eval import *

def juiPrint(*args):
    print("[print]", *args)

def juiLen(x):
    if type(x) not in [str, list]:
        raise JuiTypeError("len")
    return len(x)

R_record = RecordType("record", None)
R_subrecord = RecordType("subrecord", R_record)

builtinClosure = Closure(parent=None, scope=MutableScopeData(defs={
    "print": (0, BuiltinFunction(juiPrint)),
    "len": (0, BuiltinFunction(juiLen)),

    # TODO: Remove the built-in record type "record" used for testing
    "record": (0, R_record),
    "subrecord": (0, R_subrecord),

}, timestamp=1))
