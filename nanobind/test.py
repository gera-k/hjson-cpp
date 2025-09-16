import build.hjcpp as hjcpp
import json

test1 = """
{
    'aaa': 123 // comment aaa
    'bbb': [
        1       // comment bbb.1
        2
        3       /* comment bbb.3 */
    ] # comment bbb
    'ccc': /*comment key ccc*/ true
    'ddd': null
    'eee': {
        'fff': 'hello'
        'ggg': -1.23e-10
    }
    'hhh': 'This is a \\'test\\' string with \\\\ escapes'
}
"""

test2 = """
// This is the root object
{
    // This is 'aaa' comment
    'aaa':   123
}
"""

obj: dict = {}
comm: list = []
err: dict = {}

print("Hjson to Python:")
ret = hjcpp.hj2py(test1, obj, comm, err)
print (f"ret={ret}")
print (f"obj={json.dumps(obj, indent=4)}")
print (f"comm={json.dumps(comm, indent=4)}")
print (f"err={json.dumps(err, indent=4)}")

print("\nPython to Hjson:")
out = hjcpp.py2hj(obj, comm, err)
print (f"out={out}")
print (f"err={err}")