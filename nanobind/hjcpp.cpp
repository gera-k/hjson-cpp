#include <string>

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include "../include/hjson/hjson.h"

namespace nb = nanobind;

bool hj2py(
    std::string str,        // string containing HJSON data to parse
    nb::dict obj,           // dictionary to populate with parsed data
    nb::list comm,          // list to populate with comments
    nb::dict err            // dictionary to populate with error information
);

std::string py2hj(
    nb::dict obj,           // dictionary containing data to convert to HJSON
    nb::list comm,          // list containing comments
    nb::dict err            // dictionary to populate with error information
);

NB_MODULE(hjcpp, m) {
    m.doc() = "Python bindings for hjson-cpp\n\n"
              "  hj2py(hjson: str, obj: dict, comm: list, err: dict) -> bool\n"
              "    Parse HJSON string into Python dictionary.\n"
              "    hjson is the input HJSON string to parse.\n"
              "    obj will contain the parsed data.\n"
              "    comm will contain comments associated with the data.\n"
              "    err will contain error information if parsing fails.\n"
              "    Returns True on success, False on failure.\n"
              "    Structure of the comm:\n"
              "      comm = tuple(\n"
              "                 tuple(comment_before, comment_key, comment_inside, comment_after, pos_item, pos_key),\n"
              "                 child_comments\n"
              "             )\n"
              "     For nested structures, the 'child_comments' field will contain comments for child elements:\n"
              "        - for maps, child_comments is a dict of field_name -> comm]\n"
              "        - for arrays, child_comments is an array of comm\n"
              "      For primitive values, child_comments is None\n"
              "      Exception: the root object comm is a list[self_comments, child_comments]\n"
              "      pos_item and pos_key are the positions in the input HJSON string\n"
              "  py2hj(obj: dict, comm: list, err: dict) -> str\n"
              "    Convert Python dictionary into HJSON string.\n"
              "    obj is the data to convert.\n"
              "    comm contains comments associated with the data formatted as defined above.\n"
              "    err will contain error information if conversion fails.\n"
              "    Returns the output HJSON string or empty string in case of error.\n"
              ;

    m.def("hj2py", &hj2py, 
          nb::arg("hjson"), 
          nb::arg("obj"), 
          nb::arg("comm"), 
          nb::arg("err"),
          "Parse HJSON string into Python dictionary.");

    m.def("py2hj", &py2hj, 
          nb::arg("obj"), 
          nb::arg("comm"), 
          nb::arg("err"),
          "Convert Python dictionary into HJSON string.");
}

static void map2dict(const Hjson::Value& from, nb::dict& to, nb::dict& comm);
static void vector2list(const Hjson::Value& from, nb::list& to, nb::list& comm);
static std::function<void(std::string& key, Hjson::Value&)> duplicateKeyHandler = nullptr;

bool hj2py(
    std::string str,        // string containing HJSON data to parse
    nb::dict obj,           // dictionary to populate with parsed data
    nb::list comm,          // list to populate with comments
    nb::dict err            // dictionary to populate with error information
)
{ 
    Hjson::DecoderOptions options;
    // Keep all comments from the Hjson input, store them in
    // the Hjson::Value objects.
    options.comments = true;
    // Store all whitespace and comments in the Hjson::Value objects so that
    // linefeeds and custom indentation is kept. The "comments" option is
    // ignored if this option is true.
    options.whitespaceAsComments = true;
    // If true, an Hjson::syntax_error exception is thrown from the unmarshal
    // functions if a map contains duplicate keys.
    options.duplicateKeyException = false;

    options.duplicateKeyHandler = duplicateKeyHandler;

    try
    {
        // parse
        Hjson::Value val = Hjson::Unmarshal(str, options);

        // Convert Hjson::Value to Python dict
        if (val.type() != Hjson::Type::Map)
            throw Hjson::type_mismatch("Root is not a map");

        // comments to the root object itself
        nb::tuple comm_self = nb::make_tuple(
            val.get_comment_before(),
            val.get_comment_key(),
            val.get_comment_inside(),
            val.get_comment_after(),
            val.get_pos_item(),
            val.get_pos_key()
        ); 
        comm.append(comm_self);

        nb::dict to_comm;        
        map2dict(val, obj, to_comm);
        comm.append(to_comm);
    }
    catch (const Hjson::syntax_error& e)
    {
        err["code"] = -2;
        err["msg"] = e.what();
        return false;
    }
    catch (const Hjson::type_mismatch& e)
    {
        printf("Type mismatch: %s\n", e.what());
        err["code"] = -3;
        err["msg"] = e.what();
        return false; // Type mismatch error
    }
    catch (const Hjson::index_out_of_bounds& e)
    {
        err["code"] = -4;
        err["msg"] = e.what();
        return false; // Index out of bounds error
    }
    catch (const std::exception& e)
    {
        err["code"] = -1;
        err["msg"] = e.what();
        return false; // Other errors
    }

    return true;
}

static void vector2list(const Hjson::Value& from, nb::list& to_list, nb::list& to_comm)
{
    for (size_t i = 0; i < from.size(); ++i)
    {
        const Hjson::Value& val = from[i];

        // comments to the value itself
        nb::tuple comm_self = nb::make_tuple(
            val.get_comment_before(),
            val.get_comment_key(),
            val.get_comment_inside(),
            val.get_comment_after(),
            val.get_pos_item(),
            val.get_pos_key()
        );

        if (val.type() == Hjson::Type::Map)
        {
            nb::dict obj;
            nb::dict comm;
            map2dict(val, obj, comm);
            to_list.append(obj);
            to_comm.append(nb::make_tuple(comm_self, comm));
        }
        else if (val.type() == Hjson::Type::Vector)
        {
            nb::list arr;
            nb::list comm;
            vector2list(val, arr, comm);
            to_list.append(arr);
            to_comm.append(nb::make_tuple(comm_self, comm));
        }
        else if (val.type() == Hjson::Type::String)
        {
            to_list.append(val.to_string());
            to_comm.append(nb::make_tuple(comm_self, nb::none()));
        }
        else if (val.type() == Hjson::Type::Int64)
        {
            to_list.append(val.to_int64());
            to_comm.append(nb::make_tuple(comm_self, nb::none()));
        }
        else if (val.type() == Hjson::Type::Double)
        {
            to_list.append(val.to_double());
            to_comm.append(nb::make_tuple(comm_self, nb::none()));
        }
        else if (val.type() == Hjson::Type::Bool)
        {
            to_list.append(bool(val));
            to_comm.append(nb::make_tuple(comm_self, nb::none()));
        }
        else if (val.type() == Hjson::Type::Null)
        {
            to_list.append(nb::none());
            to_comm.append(nb::make_tuple(comm_self, nb::none()));
        }
        else
        {
            throw std::runtime_error("Unsupported Hjson value type in array");
        }
    }
}

static void map2dict(const Hjson::Value& from, nb::dict& to_dict, nb::dict& to_comm)
{
    for (auto it = from.begin(); it != from.end(); ++it)
    {
        const std::string& key = it->first;
        const Hjson::Value& val = it->second;

        nb::tuple comm_self = nb::make_tuple(
            val.get_comment_before(),
            val.get_comment_key(),
            val.get_comment_inside(),
            val.get_comment_after(),
            val.get_pos_item(),
            val.get_pos_key()
        );

        if (val.type() == Hjson::Type::Map)
        {
            nb::dict obj;       // parsed object
            nb::dict comm;      // comments of object members
            map2dict(val, obj, comm);
            to_dict[key.c_str()] = obj;
            to_comm[key.c_str()] = nb::make_tuple(comm_self, comm);
        }
        else if (val.type() == Hjson::Type::Vector)
        {
            nb::list arr;
            nb::list comm;
            vector2list(val, arr, comm);
            to_dict[key.c_str()] = arr;
            to_comm[key.c_str()] = nb::make_tuple(comm_self, comm);
        }
        else if (val.type() == Hjson::Type::String)
        {
            to_dict[key.c_str()] = val.to_string();
            to_comm[key.c_str()] = nb::make_tuple(comm_self, nb::none());
        }
        else if (val.type() == Hjson::Type::Int64)
        {
            to_dict[key.c_str()] = val.to_int64();
            to_comm[key.c_str()] = nb::make_tuple(comm_self, nb::none());
        }
        else if (val.type() == Hjson::Type::Double)
        {
            to_dict[key.c_str()] = val.to_double();
            to_comm[key.c_str()] = nb::make_tuple(comm_self, nb::none());
        }
        else if (val.type() == Hjson::Type::Bool)
        {
            to_dict[key.c_str()] = bool(val);
            to_comm[key.c_str()] = nb::make_tuple(comm_self, nb::none());
        }
        else if (val.type() == Hjson::Type::Null)
        {
            to_dict[key.c_str()] = nb::none();
            to_comm[key.c_str()] = nb::make_tuple(comm_self, nb::none());
        }
        else
        {
            throw std::runtime_error("Unsupported Hjson value type in map");
        }
    }
}

static void set_comments(Hjson::Value& to, nb::handle comm)
{
    if (comm.is_none())
        return;

    if (!nb::isinstance<nb::tuple>(comm))
        throw std::runtime_error("Comments must be a tuple");

    nb::tuple comm_tuple = nb::cast<nb::tuple>(comm);

    if (nb::len(comm) < 4)
        throw std::runtime_error("Comments tuple must have 4 elements");

    to.set_comment_before(nb::cast<std::string>(comm[0]));
    to.set_comment_key(nb::cast<std::string>(comm[1]));
    to.set_comment_inside(nb::cast<std::string>(comm[2]));
    to.set_comment_after(nb::cast<std::string>(comm[3]));
}

static Hjson::Value handle2value(
    const nb::handle& from,    // Python value to convert
    const nb::handle& comm)    // Comments associated with the value
{
    nb::tuple comm_self;
    nb::handle comm_child;
    // comm is the two-element list or tuple: [self_comments, child_comments]
    if (nb::isinstance<nb::list>(comm))
    {
        nb::list comm_list = nb::cast<nb::list>(comm);
        if (nb::len(comm_list) != 2)
            throw std::runtime_error("Comments list must have 2 elements");
        comm_self = nb::cast<nb::tuple>(comm_list[0]);
        comm_child = comm_list[1];
    }
    else if (nb::isinstance<nb::tuple>(comm))
    {
        nb::tuple comm_tuple = nb::cast<nb::tuple>(comm);
        if (nb::len(comm_tuple) != 2)
            throw std::runtime_error("Comments tuple must have 2 elements");
        comm_self = nb::cast<nb::tuple>(comm_tuple[0]);
        comm_child = comm_tuple[1];
    }
    else
    {
        throw std::runtime_error("Comments must be a list or tuple");
    }

    Hjson::Value val;

    // Convert each value to Hjson::Value
    if (from.is_none())
    {
        val = Hjson::Value(Hjson::Type::Null);
    }
    else if (nb::isinstance<bool>(from))
    {
        val = Hjson::Value(nb::cast<bool>(from));
    }
    else if (nb::isinstance<int64_t>(from))
    {
        val = Hjson::Value(nb::cast<int64_t>(from));
    }
    else if (nb::isinstance<double>(from))
    {
        val = Hjson::Value(nb::cast<double>(from));
    }
    else if (nb::isinstance<std::string>(from))
    {
        val = Hjson::Value(nb::cast<std::string>(from));
    }
    else if (nb::isinstance<nb::dict>(from))
    {
        val = Hjson::Value(Hjson::Type::Map);
        nb::dict from_dict = nb::cast<nb::dict>(from);
        nb::dict comm_dict;

        if (nb::isinstance<nb::dict>(comm_child))
            comm_dict = nb::cast<nb::dict>(comm_child);
        else
            throw std::runtime_error("Comments for map must be a dict");

        for (const auto& [key, value] : from_dict)
        {
            if (!nb::isinstance<std::string>(key))
                throw std::runtime_error("Dictionary keys must be strings");

            std::string key_str = nb::cast<std::string>(key);

            nb::handle comm_val = nb::none();
            if (comm_dict.contains(key))
                comm_val = comm_dict[key];

            val[key_str] = handle2value(value, comm_val);
        }

    }
    else if (nb::isinstance<nb::list>(from))
    {
        val = Hjson::Value(Hjson::Type::Vector);
        nb::list from_list = nb::cast<nb::list>(from);
        nb::list comm_list;

        if (nb::isinstance<nb::list>(comm_child))
            comm_list = nb::cast<nb::list>(comm_child);
        else
            throw std::runtime_error("Comments for array must be a list");

        for (size_t i = 0; i < nb::len(from_list); ++i)
        {
            nb::handle value = from_list[i];
            nb::handle comm_val = nb::none();
            if (i < nb::len(comm_list))
                comm_val = comm_list[i];
            
            val.push_back(handle2value(value, comm_val));
        }
    }
    else
    {
        throw std::runtime_error("Unsupported value type");
    }

    set_comments(val, comm_self);

    return val;
}

std::string py2hj(
    nb::dict obj,           // dictionary containing data to convert to HJSON
    nb::list comm,          // list containing comments
    nb::dict err            // dictionary to populate with error information
)
{
    Hjson::Value root;

    try
    {
        root = handle2value(obj, comm);
    }
    catch(const std::exception& e)
    {
        err["message"] = e.what();
        return "";
    }

    Hjson::EncoderOptions options;
    // End of line, should be either \n or \r\n
    options.eol = "\n";
    // Place braces on the same line
    options.bracesSameLine = true;
    // Always place string values in double quotation marks ("), and escape
    // any special chars inside the string value
    options.quoteAlways = false;
    // Always place keys in quotes
    options.quoteKeys = false;
    // Indent string
    options.indentBy = "  ";
    // Allow the -0 value (unlike ES6)
    options.allowMinusZero = false;
    // Encode unknown values as 'null'
    options.unknownAsNull = false;
    // Output a comma separator between elements. If true, always place strings
    // in quotes (overriding the "quoteAlways" setting).
    options.separator = false;
    // Only affects the order of elements in objects. If true, the key/value
    // pairs for all objects will be placed in the same order as they were added.
    // If false, the key/value pairs are placed in alphabetical key order.
    options.preserveInsertionOrder = true;
    // If true, omits root braces.
    options.omitRootBraces = false;
    // Write comments, if any are found in the Hjson::Value objects.
    options.comments = true;

    try
    {
        return Hjson::Marshal(root, options);
    }
    catch (const std::exception& e)
    {
        err["message"] = e.what();
        return "";
    }

    return "";
}
            