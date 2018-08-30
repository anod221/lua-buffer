# lua-buffer
AS3 ByteArray module for lua.

# usage: 
To add this module to lvm, call luaopen_bytearr when initialize the lua aux lib.

```lua
luaopen_bytearr(L);
```

# constant
```lua
ByteArray.LITTLE_ENDIAN = 0
ByteArray.BIG_ENDIAN    = 1
```

# constructor
`create( [capacity, [endian]] )` Create an empty byte array.   
`init( [param1, param2, ...] )` Create a byte array with initial bytes. Parameters should be a list of number or table.   
`load( str )` Create a byte array with lua string object for read data. This byte array is read-only.   

`return` A ByteArray object.

```lua
local x = ByteArray.create()  -- create an empty byte array
local y = ByteArray.init( 0, 1, 2, 3 ) -- create a byte array with initial bytes
local z = ByteArray.load( "hello" ) -- create a byte array with a lua string object
```

# reading data
`readByte()` Read a 8-bit signed integer from byte array.   
`readUnsignedByte()` Read a 8-bit unsigned integer from byte array.   
`readShort()` Read a 16-bit signed integer from byte array.   
`readUnsignedShort()` Read a 16-bit unsigned integer from byte array.   
`readInt()` Read a 32-bit signed integer from byte array.   
`readUnsignedInt()` Read a 32-bit unsigned integer from byte array.   
`readFloat()` Read a 32-bit float from byte array.   
`readDouble()` Read a 64-bit float from byte array.   
`readCString()` Read a string end with \0 from byte array.   

`return` Data read.

```lua
local buf = ByteArray.init( 1, 2, 3, 4, 5, 6 ) 
local a = buf:readByte() -- a == 1 
local b = buf:readUnsignedByte() -- b == 2 
local c = buf:readShort() -- c == 0x0403 
```

# writing data
`writeByte( s8 )` Write a 8-bit signed integer to byte array.   
`writeUnsignedByte( u8 )` Write a 8-bit unsigned integer to byte array.   
`writeShort( s16 )` Write a 16-bit signed integer to byte array.   
`writeUnsignedShort( u16 )` Write a 16-bit unsigned integer to byte array.   
`writeInt( s32 )` Write a 32-bit signed integer to byte array.   
`writeUnsignedInt( u32 )` Write a 32-bit unsigned integer to byte array.   
`writeFloat( f32 )` Write a 32-bit float to byte array.   
`writeDouble( f64 )` Write a 64-bit float to byte array.   
`writeCString( str )` write a string end with \0 to byte array.   

`return` The ByteArray object itself.

```lua
local buf = ByteArray.create()
buf:writeByte(1):writeByte(2):writeInt(0x0403)
```

# copying data between ByteArray object
`readBytes( to[, offset, length] )` Read data to first parameter(a ByteArray object), the range for the target byte array is start from offset with length.    
`writeBytes( from[, offset, length] )` Write data from first parameter(a ByteArray object), the range for the data is start from offset with length.    

`return` nil

```lua
local buf = ByteArray.init( 1, 2, 3, 4, 5 )
local d = ByteArray.create()
buf:readBytes( d )
```

# clearing the ByteArray object
`clear()` Reset the object to an empty byte array.

`return` nil

```lua
local buf = ByteArray.init(1,2,3,4,5)
buf:clear()
print( buf.length ) -- 0
```

# converting the ByteArray object to lua string
`toString()` Convert the object to a lua string which allow \0 inside.

`return` A lua string.

```lua
local buf = ByteArray.init( 49, 50, 51, 52, 53, 0, 54, 55 )
local str = buf:toString()
print( str, #str, string.byte(str, 6 ), string.byte(str, 7) )
```

# slicing the array
`slice(position_stat, position_end)` The same behavior as slice in ECMAScript.

`return` A new ByteArray object.

```lua
local buf = ByteArray.init( 1,2,3,4,5,6 )
local sub = buf:slice( 1, -1 )
for i=1, #sub do print(sub[i]) end -- 2 3 4 5
```

# member position
Start position for reading / writing data. Position is start from 0 to length.

```lua
local buf = ByteArray.init( 1, 2, 3, 4, 5 )
buf.position = 3
local d = ByteArray.create()
buf:readBytes( d ) -- buf = <4 5>
print( buf.position ) -- 5
```

# member length
Set / get length.

```lua
local buf = ByteArray.init( 1,2,3,4,5,6,7 )
print( buf.length )
buf.length = 4
for i=1, #buf do print(buf[i]) end -- 1 2 3 4
```

# member endian
Set / get endian.

```lua
local buf = ByteArray.init(2,3)
buf.endian = ByteArray.LITTLE_ENDIAN
```

# indexing
Index start from 1 to length. If out of range, return -1.

```lua
local buf = ByteArray.init(2,3)
print(buf[1]) -- 2
print(buf[2]) -- 3
print(buf[3]) -- -1
```
