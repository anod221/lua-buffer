# lua-buffer
AS3 ByteArray class for lua.

#usage
add the module:
`
luaopen_bytearr(L);
`

## constructor
create( [capacity, [endian]] ) - Create an empty byte array.
init( [param1, param2, ...] ) - Create a byte array with initial bytes. Parameters should be a list of number or table.
load( str ) - Create a byte array with lua string object for read data. This byte array is read-only.

return - A ByteArray object.
`
local x = ByteArray.create()  -- create an empty byte array
local y = ByteArray.init( 0, 1, 2, 3 ) -- create a byte array with initial bytes
local z = ByteArray.load( "hello" ) -- create a byte array with a lua string object
`

## method for read data
readByte() - Read a 8-bit signed integer from byte array.
readUnsignedByte() - Read a 8-bit unsigned integer from byte array.
readShort() - Read a 16-bit signed integer from byte array.
readUnsignedShort() - Read a 16-bit unsigned integer from byte array.
readInt() - Read a 32-bit signed integer from byte array.
readUnsignedInt() - Read a 32-bit unsigned integer from byte array.
readFloat() - Read a 32-bit float from byte array.
readDouble() - Read a 64-bit float from byte array.
readCString() - Read a string end with \0 from byte array.

return - A lua number object.
`
local t = ByteArray.init( 1, 2, 3, 4, 5, 6 )
local a = t:readByte() -- a == 1
local b = t:readUnsignedByte() -- b == 2
local c = t:readShort() -- c == 0x0403
`

## method for write data
writeByte( s8 ) - Write a 8-bit signed integer to byte array.
writeUnsignedByte( u8 ) - Write a 8-bit unsigned integer to byte array.
writeShort( s16 ) - Write a 16-bit signed integer to byte array.
writeUnsignedShort( u16 ) - Write a 16-bit unsigned integer to byte array.
writeInt( s32 ) - Write a 32-bit signed integer to byte array.
writeUnsignedInt( u32 ) - Write a 32-bit unsigned integer to byte array.
writeFloat( f32 ) - Write a 32-bit float to byte array.
writeDouble( f64 ) - Write a 64-bit float to byte array.
writeCString( str ) - write a string end with \0 to byte array.

return - The ByteArray object itself.
`
local buf = ByteArray.create()
buf:writeByte(1):writeByte(2):writeInt(0x0403)
`

## method for copying data between ByteArray object
readBytes( to[, offset, length] ) - Read data to first parameter(a ByteArray object), the range for the target byte array is start from offset with length.
writeBytes( from[, offset, length] ) - Write data from first parameter(a ByteArray object), the range for the data is start from offset with length.
`
local buf = ByteArray.init( 1, 2, 3, 4, 5 )
local d = ByteArray.create()
buf:readBytes( d )
`

## member position
start position for reading / writing data.
`
local buf = ByteArray.init( 1, 2, 3, 4, 5 )
buf.position = 3
local d = ByteArray.create()
buf:readBytes( d ) -- buf = <4 5>
print( buf.position ) -- 5
`

## member length
set / get length
`
local buf = ByteArray.init( 1,2,3,4,5,6,7 )
print( buf.length )
buf.length = 4
for i=1, #buf do print(buf[i]) end
`

## member endian
set / get endian
`
local buf = ByteArray.init(2,3)
buf.endian = ByteArray.LITTLE_ENDIAN
`

## indexing
index start from 1 to length. If out of range, return -1.
`
local buf = ByteArray.init(2,3)
print(buf[1])
print(buf[2])
`
