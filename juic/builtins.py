from juic.datatypes import *
from juic.eval import *

def juiPrint(*args):
    print("[print]", *args)

def juiLen(x):
    if type(x) not in [str, list]:
        raise JuiTypeError("len")
    return len(x)

builtinClosure = Closure(parent=None, scope=MutableScopeData(defs={
    "print": (0, BuiltinFunction(juiPrint)),
    "len": (0, BuiltinFunction(juiLen)),

    # TODO: Remove the built-in record type "record" used for testing
    "record": (0, RecordType.makePlainRecordType()),
    "subrecord": (0, RecordType.makePlainRecordType()),

}, timestamp=1))
