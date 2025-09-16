import build.hjcpp as hjcpp
import json
from collections import OrderedDict

test1 = """
{
    'hhh': 'This is a \\'test\\' string with \\\\ escapes'
    'aaa': 123 // comment aaa
    'bbb': [
        1       // comment bbb.1
        2
        3       /* comment bbb.3 */
    ] # comment bbb
    'ccc': /*comment key ccc*/ true
    'eee': {
        'ggg': -1.23e-10
        'fff': 'hello'
    }
    'ddd': null
}
"""

test2 = """
// This is the root object
{
    // This is 'aaa' comment
    'aaa':   123
}
"""

test3 = """
{
    cfg: {
        cfg: This is config blok 0
        val: 1234
    }
    cfg: {
        cfg: This is config blok 1
        val: 5678
    }
}
"""

test = test1

# obj: OrderedDict = OrderedDict()
obj: dict = {}
comm: list = []
err: dict = {}

print("Hjson to Python:")
ret = hjcpp.hj2py(test, obj, comm, err)
print (f"ret={ret}")
print (f"obj={json.dumps(obj, indent=4)}")
print (f"comm={json.dumps(comm, indent=4)}")
print (f"err={json.dumps(err, indent=4)}")

print("\nPython to Hjson:")
out = hjcpp.py2hj(obj, comm, err)
print (f"out={out}")
print (f"err={err}")

print (f"\nHjson library version: {hjcpp.version()}")
