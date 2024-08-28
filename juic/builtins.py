from juic.datatypes import *
from juic.eval import *

def juiPrint(*args):
    print("[print]", *(juiValueString(a) for a in args))

def juiLen(x):
    if type(x) not in [str, list]:
        raise JuiTypeError("len")
    return len(x)

R_record = RecordType("record", None)
R_subrecord = RecordType("subrecord", R_record)
R_jwidget = RecordType("jwidget", None)
R_jlabel = RecordType("jlabel", R_jwidget)
R_jfkeys = RecordType("jfkeys", R_jwidget)

builtinClosure = Closure(parent=None, scope=MutableScopeData(defs={
    "print": (0, BuiltinFunction(juiPrint)),
    "len": (0, BuiltinFunction(juiLen)),

    # TODO: Remove the built-in record type "record" used for testing
    "record": (0, R_record),
    "subrecord": (0, R_subrecord),
    "jwidget": (0, R_jwidget),
    "jlabel": (0, R_jlabel),
    "jfkeys": (0, R_jfkeys),

}, timestamp=1))
