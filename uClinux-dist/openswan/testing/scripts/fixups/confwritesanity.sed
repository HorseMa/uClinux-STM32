/^#setting/d
s;\(opening file: \).*\(testing/.*\);\1DIR/\2;
s;\(including file \).*\(testing/.* from line \).*\(testing/.*:.*\);\1DIR/\2DIR/\3;
s;\(end of file \).*\(testing/.*, resuming \).*\(testing/.* line .*\);\1DIR/\2DIR/\3;
s;\(resuming \).*\(/testing/.* line .*\);\1DIR\2;
s;\(could not open include filename: .* (tried \).*\(/testing.* and \).*\(/testing.*)\);\1DIR\2DIR\3;
s;\(could not open include filename: .* (tried \).*\(/testing.* and )\);\1DIR\2;
s;\(end of file \).*\(, resuming <none> line -1\);\1DIR\2;
s;\(end of file \).*$;\1DIR;
s;\(config file: \).*\( can not be loaded: can.t load file '\).*\('\);\1DIR\2DIR\3;
