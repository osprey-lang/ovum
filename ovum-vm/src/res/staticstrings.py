import json
import collections

# In all the string formats, we use the following placeholders:
#   {0} = indentation (tabs)
#   {1} = unqualified member or group name (simple identifier)
#   {2} = dotted member path, including the member's own name
#   {3} = underscored member path, including the member's own name
#   {4} = length of string in UTF-16 code units (not available for groups)
#   {5} = string data, as a string of comma-separated UTF-16 code units,
#         with a trailing zero (not available for groups)
CONFIG = {
  'INPUT_FILE': 'staticstrings.json',
  'HEADER': {
    'PLACEHOLDERS': {
      'DATA_MEMBERS': '/*@DataMembers@*/',
      'STRING_MEMBERS': '/*@StringMembers@*/',
    },
    'FORMATS': {
      'DATA_MEMBER': '{0}LitString<{4}> {3};',
      'STRING_MEMBER': '{0}::String *{1};',
      'GROUP_START': '{0}struct {1}Strings {{',
      'GROUP_END': '{0}}} {1};',
    },
    'TEMPLATE': 'staticstrings.template.h',
    'OUTPUT': 'staticstrings.h',
  },
  'SOURCE': {
    'PLACEHOLDERS': {
      'DATA_VALUES': '/*@StringData@*/',
      'STRING_INITIALIZERS': '/*@StringInitializers@*/'
    },
    'FORMATS': {
      'DATA_VALUE': '{0}{{ {4}, 0, StringFlags::STATIC, {5} }},',
      'STRING_INITIALIZER': '{0}this->{2} = reinterpret_cast<String*>(&data->{3});',
    },
    'TEMPLATE': 'staticstrings.template.cpp',
    'OUTPUT': 'staticstrings.cpp',
  },
}

def generate_files(config):
  string_data = _parse_json(config['INPUT_FILE'])
  header_config = config['HEADER']
  source_config = config['SOURCE']

  members = init_members(string_data)

  header_template = _read_all_text(header_config['TEMPLATE'])
  formatted_header = format_header(
      header_template,
      members,
      header_config['PLACEHOLDERS'],
      header_config['FORMATS']
    )

  source_template = _read_all_text(source_config['TEMPLATE'])
  formatted_source = format_source(
      source_template,
      members,
      source_config['PLACEHOLDERS'],
      source_config['FORMATS']
    )

  _write_all_text(header_config['OUTPUT'], formatted_header)
  _write_all_text(source_config['OUTPUT'], formatted_source)

class MemberDefinition:
  def __init__(self, path, name):
    self.path = path
    self.name = name

  def dotted_path(self):
    return self.joined_path('.')

  def underscored_path(self):
    return self.joined_path('_')

  def joined_path(self, sep):
    return sep.join(self.path + (self.name,))

class GroupDefinition(MemberDefinition):
  def __init__(self, path, name, children):
    super(GroupDefinition, self).__init__(path, name)
    self.group = True
    self.children = children
    self._format_args = None

  def __iter__(self):
    yield from self.children

  def format_args(self):
    if self._format_args is None:
      self._format_args = (self.name, self.dotted_path(), self.underscored_path())
    return self._format_args

class StringDefinition(MemberDefinition):
  def __init__(self, path, name, value):
    super(StringDefinition, self).__init__(path, name)
    self.group = False
    self.value = value
    self._format_args = None

  def format_args(self):
    if self._format_args is None:
      utf16 = self.to_utf16()
      self._format_args = (
          self.name,
          self.dotted_path(),
          self.underscored_path(),
          len(utf16),
          ','.join(map(str, utf16 + [0]))
        )
    return self._format_args

  def to_utf16(self):
    utf16_bytes = self.value.encode('utf-16le');
    code_units = []
    for i in range(0, len(utf16_bytes), 2):
      code_units.append(utf16_bytes[i] + (utf16_bytes[i + 1] << 8))
    return code_units

def init_members(string_data):
  def get_members(path, entries):
    members = []

    for k, v in entries.items():
      if isinstance(v, dict):
        group_children = get_members(path + (k,), v)
        member = GroupDefinition(path, k, group_children)
      else:
        member = StringDefinition(path, k, v)
      members.append(member)

    return members

  return get_members((), string_data)

def _iter_flattened(members):
  for m in members:
    if m.group:
      yield from _iter_flattened(m.children)
    else:
      yield m

def format_header(template, members, placeholders, formats):
  data_members = []
  string_members = []
  data_member_format = formats['DATA_MEMBER']
  string_member_format = formats['STRING_MEMBER']
  group_start_format = formats['GROUP_START']
  group_end_format = formats['GROUP_END']

  # Data members
  for member in _iter_flattened(members):
    format_args = ('\t',) + member.format_args()
    data_members.append(data_member_format.format(*format_args))

  # String members
  def format_string_member(member, indent):
    format_args = (indent,) + member.format_args()
    if member.group:
      string_members.append(group_start_format.format(*format_args))
      for m in member:
        format_string_member(m, '\t' + indent)
      string_members.append(group_end_format.format(*format_args))
    else:
      string_members.append(string_member_format.format(*format_args))

  for member in members:
    format_string_member(member, '\t')

  template = template.replace(placeholders['DATA_MEMBERS'], '\n'.join(data_members))
  template = template.replace(placeholders['STRING_MEMBERS'], '\n'.join(string_members))

  return template

def format_source(template, members, placeholders, formats):
  data_values = []
  string_initers = []

  data_value_format = formats['DATA_VALUE']
  string_initer_format = formats['STRING_INITIALIZER']

  for member in _iter_flattened(members):
    format_args = ('\t',) + member.format_args()
    data_values.append(data_value_format.format(*format_args))
    string_initers.append(string_initer_format.format(*format_args))

  template = template.replace(placeholders['DATA_VALUES'], '\n'.join(data_values))
  template = template.replace(placeholders['STRING_INITIALIZERS'], '\n'.join(string_initers))

  return template

def _parse_json(filename):
  with open(filename, encoding='utf-8') as f:
    # We need to order the keys so that the data values are guaranteed
    # to correspond to the data member order, otherwise you get all kinds
    # of fun problems. Hence: OrderedDict!
    result = json.load(f, object_pairs_hook=collections.OrderedDict)
  return result

def _read_all_text(filename):
  with open(filename, encoding='utf-8') as f:
    file_text = f.read()
  return file_text

def _write_all_text(filename, text):
  with open(filename, mode='w', encoding='utf-8', newline='') as f:
    f.write(text)

if __name__ == '__main__':
  generate_files(CONFIG);
