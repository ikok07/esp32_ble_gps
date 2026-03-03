| Name | Type                                                            | Size (Bytes) | Range              | Resolution      |
|------|-----------------------------------------------------------------|--------------|--------------------|-----------------|
| U1   | unsigned 8-bit integer                                          | 1            | 0 … 2^8 - 1        | 1               |
| I1   | signed 8-bit integer (two's complement)                         | 1            | -2^7 … 2^7 - 1     | 1               |
| X1   | 8-bit bitfield                                                  | 1            | n/a                | n/a             |
| U2   | unsigned little-endian 16-bit integer                           | 2            | 0 … 2^16 - 1       | 1               |
| I2   | signed little-endian 16-bit integer (two's complement)          | 2            | -2^15 … 2^15 - 1   | 1               |
| X2   | 16-bit little-endian bitfield                                   | 2            | n/a                | n/a             |
| U4   | unsigned little-endian 32-bit integer                           | 4            | 0 … 2^32 - 1       | 1               |
| I4   | signed little-endian 32-bit integer (two's complement)          | 4            | -2^31 … 2^31 - 1   | 1               |
| X4   | 32-bit little-endian bitfield                                   | 4            | n/a                | n/a             |
| R4   | IEEE 754 single (32-bit) precision                              | 4            | ~ -2^127 … 2^127   | ~ value · 2^-24 |
| R8   | IEEE 754 double (64-bit) precision                              | 8            | ~ -2^1023 … 2^1023 | ~ value · 2^-53 |
| CH   | ASCII / ISO 8859-1 char (8-bit)                                 | 1            | n/a                | n/a             |
| U:n  | unsigned bitfield value of n bits width                         | var.         | variable           | variable        |
| I:n  | signed (two's complement) bitfield value of n bits width        | var.         | variable           | variable        |
| S:n  | signed bitfield value of n bits width (sign-magnitude notation) | var.         | variable           | variable        |
| L    | Boolean                                                         | 1            | 0 ... 1            | 1               |