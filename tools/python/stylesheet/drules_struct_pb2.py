# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: drules_struct.proto
"""Generated protocol buffer code."""
from google.protobuf import descriptor as _descriptor
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf import symbol_database as _symbol_database
from google.protobuf.internal import builder as _builder
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(b'\n\x13\x64rules_struct.proto\"*\n\x0c\x44\x61shDotProto\x12\n\n\x02\x64\x64\x18\x01 \x03(\x01\x12\x0e\n\x06offset\x18\x02 \x01(\x01\":\n\x0cPathSymProto\x12\x0c\n\x04name\x18\x01 \x01(\t\x12\x0c\n\x04step\x18\x02 \x01(\x01\x12\x0e\n\x06offset\x18\x03 \x01(\x01\"\xaf\x01\n\rLineRuleProto\x12\r\n\x05width\x18\x01 \x01(\x01\x12\r\n\x05\x63olor\x18\x02 \x01(\r\x12\x1e\n\x07\x64\x61shdot\x18\x03 \x01(\x0b\x32\r.DashDotProto\x12\x10\n\x08priority\x18\x04 \x01(\x05\x12\x1e\n\x07pathsym\x18\x05 \x01(\x0b\x32\r.PathSymProto\x12\x17\n\x04join\x18\x06 \x01(\x0e\x32\t.LineJoin\x12\x15\n\x03\x63\x61p\x18\x07 \x01(\x0e\x32\x08.LineCap\"\x9c\x01\n\x0cLineDefProto\x12\r\n\x05width\x18\x01 \x01(\x01\x12\r\n\x05\x63olor\x18\x02 \x01(\r\x12\x1e\n\x07\x64\x61shdot\x18\x03 \x01(\x0b\x32\r.DashDotProto\x12\x1e\n\x07pathsym\x18\x04 \x01(\x0b\x32\r.PathSymProto\x12\x17\n\x04join\x18\x06 \x01(\x0e\x32\t.LineJoin\x12\x15\n\x03\x63\x61p\x18\x07 \x01(\x0e\x32\x08.LineCap\"O\n\rAreaRuleProto\x12\r\n\x05\x63olor\x18\x01 \x01(\r\x12\x1d\n\x06\x62order\x18\x02 \x01(\x0b\x32\r.LineDefProto\x12\x10\n\x08priority\x18\x03 \x01(\x05\"_\n\x0fSymbolRuleProto\x12\x0c\n\x04name\x18\x01 \x01(\t\x12\x16\n\x0e\x61pply_for_type\x18\x02 \x01(\x05\x12\x10\n\x08priority\x18\x03 \x01(\x05\x12\x14\n\x0cmin_distance\x18\x04 \x01(\x05\"\x8d\x01\n\x0f\x43\x61ptionDefProto\x12\x0e\n\x06height\x18\x01 \x01(\x05\x12\r\n\x05\x63olor\x18\x02 \x01(\r\x12\x14\n\x0cstroke_color\x18\x03 \x01(\r\x12\x10\n\x08offset_x\x18\x04 \x01(\x05\x12\x10\n\x08offset_y\x18\x05 \x01(\x05\x12\x0c\n\x04text\x18\x06 \x01(\t\x12\x13\n\x0bis_optional\x18\x07 \x01(\x08\"l\n\x10\x43\x61ptionRuleProto\x12!\n\x07primary\x18\x01 \x01(\x0b\x32\x10.CaptionDefProto\x12#\n\tsecondary\x18\x02 \x01(\x0b\x32\x10.CaptionDefProto\x12\x10\n\x08priority\x18\x03 \x01(\x05\"a\n\x0f\x43ircleRuleProto\x12\x0e\n\x06radius\x18\x01 \x01(\x01\x12\r\n\x05\x63olor\x18\x02 \x01(\r\x12\x1d\n\x06\x62order\x18\x03 \x01(\x0b\x32\r.LineDefProto\x12\x10\n\x08priority\x18\x04 \x01(\x05\"m\n\x11PathTextRuleProto\x12!\n\x07primary\x18\x01 \x01(\x0b\x32\x10.CaptionDefProto\x12#\n\tsecondary\x18\x02 \x01(\x0b\x32\x10.CaptionDefProto\x12\x10\n\x08priority\x18\x03 \x01(\x05\"\x9d\x01\n\x0fShieldRuleProto\x12\x0e\n\x06height\x18\x01 \x01(\x05\x12\r\n\x05\x63olor\x18\x02 \x01(\r\x12\x14\n\x0cstroke_color\x18\x03 \x01(\r\x12\x10\n\x08priority\x18\x04 \x01(\x05\x12\x14\n\x0cmin_distance\x18\x05 \x01(\x05\x12\x12\n\ntext_color\x18\x06 \x01(\r\x12\x19\n\x11text_stroke_color\x18\x07 \x01(\r\"\xa1\x02\n\x10\x44rawElementProto\x12\r\n\x05scale\x18\x01 \x01(\x05\x12\x1d\n\x05lines\x18\x02 \x03(\x0b\x32\x0e.LineRuleProto\x12\x1c\n\x04\x61rea\x18\x03 \x01(\x0b\x32\x0e.AreaRuleProto\x12 \n\x06symbol\x18\x04 \x01(\x0b\x32\x10.SymbolRuleProto\x12\"\n\x07\x63\x61ption\x18\x05 \x01(\x0b\x32\x11.CaptionRuleProto\x12 \n\x06\x63ircle\x18\x06 \x01(\x0b\x32\x10.CircleRuleProto\x12%\n\tpath_text\x18\x07 \x01(\x0b\x32\x12.PathTextRuleProto\x12 \n\x06shield\x18\x08 \x01(\x0b\x32\x10.ShieldRuleProto\x12\x10\n\x08\x61pply_if\x18\t \x03(\t\"G\n\x13\x43lassifElementProto\x12\x0c\n\x04name\x18\x01 \x01(\t\x12\"\n\x07\x65lement\x18\x02 \x03(\x0b\x32\x11.DrawElementProto\"F\n\x11\x43olorElementProto\x12\x0c\n\x04name\x18\x01 \x01(\t\x12\r\n\x05\x63olor\x18\x02 \x01(\r\x12\t\n\x01x\x18\x03 \x01(\x02\x12\t\n\x01y\x18\x04 \x01(\x02\"7\n\x12\x43olorsElementProto\x12!\n\x05value\x18\x01 \x03(\x0b\x32\x12.ColorElementProto\"Y\n\x0e\x43ontainerProto\x12\"\n\x04\x63ont\x18\x01 \x03(\x0b\x32\x14.ClassifElementProto\x12#\n\x06\x63olors\x18\x02 \x01(\x0b\x32\x13.ColorsElementProto*4\n\x08LineJoin\x12\r\n\tROUNDJOIN\x10\x00\x12\r\n\tBEVELJOIN\x10\x01\x12\n\n\x06NOJOIN\x10\x02*3\n\x07LineCap\x12\x0c\n\x08ROUNDCAP\x10\x00\x12\x0b\n\x07\x42UTTCAP\x10\x01\x12\r\n\tSQUARECAP\x10\x02\x42\x02H\x03\x62\x06proto3')

_globals = globals()
_builder.BuildMessageAndEnumDescriptors(DESCRIPTOR, _globals)
_builder.BuildTopDescriptorsAndMessages(DESCRIPTOR, 'drules_struct_pb2', _globals)
if _descriptor._USE_C_DESCRIPTORS == False:

  DESCRIPTOR._options = None
  DESCRIPTOR._serialized_options = b'H\003'
  _globals['_LINEJOIN']._serialized_start=1851
  _globals['_LINEJOIN']._serialized_end=1903
  _globals['_LINECAP']._serialized_start=1905
  _globals['_LINECAP']._serialized_end=1956
  _globals['_DASHDOTPROTO']._serialized_start=23
  _globals['_DASHDOTPROTO']._serialized_end=65
  _globals['_PATHSYMPROTO']._serialized_start=67
  _globals['_PATHSYMPROTO']._serialized_end=125
  _globals['_LINERULEPROTO']._serialized_start=128
  _globals['_LINERULEPROTO']._serialized_end=303
  _globals['_LINEDEFPROTO']._serialized_start=306
  _globals['_LINEDEFPROTO']._serialized_end=462
  _globals['_AREARULEPROTO']._serialized_start=464
  _globals['_AREARULEPROTO']._serialized_end=543
  _globals['_SYMBOLRULEPROTO']._serialized_start=545
  _globals['_SYMBOLRULEPROTO']._serialized_end=640
  _globals['_CAPTIONDEFPROTO']._serialized_start=643
  _globals['_CAPTIONDEFPROTO']._serialized_end=784
  _globals['_CAPTIONRULEPROTO']._serialized_start=786
  _globals['_CAPTIONRULEPROTO']._serialized_end=894
  _globals['_CIRCLERULEPROTO']._serialized_start=896
  _globals['_CIRCLERULEPROTO']._serialized_end=993
  _globals['_PATHTEXTRULEPROTO']._serialized_start=995
  _globals['_PATHTEXTRULEPROTO']._serialized_end=1104
  _globals['_SHIELDRULEPROTO']._serialized_start=1107
  _globals['_SHIELDRULEPROTO']._serialized_end=1264
  _globals['_DRAWELEMENTPROTO']._serialized_start=1267
  _globals['_DRAWELEMENTPROTO']._serialized_end=1556
  _globals['_CLASSIFELEMENTPROTO']._serialized_start=1558
  _globals['_CLASSIFELEMENTPROTO']._serialized_end=1629
  _globals['_COLORELEMENTPROTO']._serialized_start=1631
  _globals['_COLORELEMENTPROTO']._serialized_end=1701
  _globals['_COLORSELEMENTPROTO']._serialized_start=1703
  _globals['_COLORSELEMENTPROTO']._serialized_end=1758
  _globals['_CONTAINERPROTO']._serialized_start=1760
  _globals['_CONTAINERPROTO']._serialized_end=1849
# @@protoc_insertion_point(module_scope)
