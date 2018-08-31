
local function test_read_cstr()
   local buffer_data = {
      0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x77, 0x6f,
      0x72, 0x6c, 0x64, 0,
      0,
      0x62, 0x79, 0x74, 0x65, 0x61, 0x72, 0x72, 0x61,
      0x79, 0
   }
   local buf = ByteArray.init( buffer_data )
   assert( buf:readCString() == "hello,world" )
   assert( buf:readCString() == "" )
   assert( buf:readCString() == "bytearray")
end

local function test_read_integer(  )
   local buffer_data = {
      0x0, 0x1, 0x94, 0xda,	-- unsigned byte
      224, -9,			-- byte
      0x03, 0xaa, 0xf4, 0x11, 	-- unsigned short
      0x45, 0xb2, 0x75, 0x00,	-- short
      0x00, 0x00, 0x00, 0x50,	-- unsigned int
      0x02, 0x04, 0x06, 0x80,
      0x00, 0x00, 0x00, 0x80,
      0xff, 0xff, 0xff, 0xff
   }
   local buf = ByteArray.init( buffer_data )
   
   assert( buf:readUnsignedByte() == 0 )
   assert( buf:readUnsignedByte() == 1 )
   assert( buf:readUnsignedByte() == 0x94 )
   assert( buf:readUnsignedByte() == 0xda )

   assert( buf:readByte() == 224-0x100 ) -- 224-256
   assert( buf:readByte() == -9 )

   assert( buf:readUnsignedShort() == 0xaa03 )
   assert( buf:readUnsignedShort() == 0x11f4 )

   assert( buf:readShort() == 0xb245-0x10000 ) -- 0xb
   assert( buf:readShort() == 0x75 )

   assert( buf:readUnsignedInt() == 0x50000000 )
   assert( buf:readUnsignedInt() == 0x80060402 )

   assert( buf:readInt() == -2147483648 )
   assert( buf:readInt() == -1 )
end

local function test_read_float()
   local buffer_data = {
      0x00, 0x00, 0x00, 0x3f,	-- 0.5f from nodejs buffer
      0xcd, 0xcc, 0xcc, 0x3d,	-- 0.1f from nodejs buffer
      0xda, 0x0f, 0x49, 0x40	-- 3.1415925 from nodejs buffer
   }
   local buf = ByteArray.init( buffer_data );
   assert( math.abs(buf:readFloat() - 0.5) < 0.0001 )
   assert( math.abs(buf:readFloat() - 0.1) < 0.0001 )
   assert( math.abs(buf:readFloat() - math.pi) < 0.0001 )
end

local function test_endian()
   local le = ByteArray.create( 16, ByteArray.LITTLE_ENDIAN )
   local be = ByteArray.create( 16, ByteArray.BIG_ENDIAN )
   le:writeInt( 0x20180901 )
   be:writeInt( 0x20180901 )
   assert( le[1] == 1 )
   assert( le[2] == 9 )
   assert( le[3] == 0x18 )
   assert( le[4] == 0x20 )
   assert( be[1] == 0x20 )
   assert( be[2] == 0x18 )
   assert( be[3] == 9 )
   assert( be[4] == 1 )
end

local function test_index()
   local buf = ByteArray.init( {0x00, 0xab, 0xcd, 0xef, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55} )

   -- read index
   assert( buf[1] == 0 )
   assert( buf[2] == 0xab )
   assert( buf[3] == 0xcd )
   assert( buf[4] == 0xef )
   for i=7, #buf do
      assert( buf[i] == buf[i-1] + buf[i-2] )
   end
   
   assert( buf[0] == -1 )   
   assert( buf[1+#buf] == -1 )
   assert( buf[-1] == -1 )
   assert( buf[0.1] == -1 )
   assert( buf[1.1] == -1 )
   assert( buf[3.9] == -1 )
   assert( buf[4.5] == -1 )

   -- write index
   local buffer_data = "abcdefghijklmn" -- #buf == #buffer_data == 14
   for i=1, #buf do
      buf[i] = string.byte(buffer_data, i)
   end
   assert( buf:toString() == buffer_data )
   buf[1+#buf] = string.byte("z", 1)
   assert( buf:toString() == buffer_data .. "z" )
end

local function test_tostring()
   local buffer_data_str = "Hello, ByteArray!"
   local buffer_data = {}
   for i=1, #buffer_data_str do
      buffer_data[1+#buffer_data] = string.byte(buffer_data_str, i)
   end

   local buf = ByteArray.init( buffer_data )
   assert( buf:toString() == buffer_data_str )
end

local function test_property_length()
   local buf
   buf = ByteArray.create()
   assert( buf.length == 0 )
   buf = ByteArray.create(8)
   assert( buf.length == 0 )
   buf = ByteArray.load("hello")
   assert( buf.length == #"hello" )
   buf = ByteArray.init( 1 )
   assert( buf.length == 1 )
   buf[2] = 1
   assert( buf.length == 2 )
   buf[11] = 9
   assert( buf.length == 11 )
   buf = ByteArray.init( 48,49,50,51,52,53,54,55,56,57 )
   buf.length = 4
   assert( buf.length == 4 )
   assert( buf:toString() == "0123" )
   buf.length = 6		-- scale
   assert( buf[1] == 48 )
   assert( buf[2] == 49 )
   assert( buf[3] == 50 )
   assert( buf[4] == 51 )
   assert( buf[5] == 0 )
   assert( buf[6] == 0 )
   assert( buf[7] == -1 )
end

local function test_property_position()
   local buf = ByteArray.init(1,2,3,4,5)
   assert( buf.position == 0 )
   buf:readUnsignedByte()
   assert( buf.position == 1 )
   buf.position = 3
   assert( buf.position == 3 )
   buf.position = 0
   assert( buf.position == 0 )
   assert( not pcall( function() buf.position = -1 end ) )
   buf.position = #buf+1
   assert( buf.position == #buf )
   buf.position = #buf
   assert( buf.position == #buf )
end

local function test_readonly()
   local robuf = ByteArray.load("hello")

   assert( pcall( function() robuf.position = 4 end ) ) -- allow setting position
   assert( pcall( function() robuf.endian = ByteArray.BIG_ENDIAN end ) ) -- allow setting endian
   assert( not pcall( function() robuf:writeByte(9) end ) ) -- forbid writing
   assert( not pcall( function() robuf.length = 2 end ) )   -- forbid setting length
   assert( not pcall( function() robuf[1] = 3 end ) )	    -- forbid index writing
end

local function test_read_bytes()
   local buffer_data = {
      11, 22, 33, 44, 55, 66, 77, 88
   }
   local src = ByteArray.init( buffer_data );
   assert( src:readByte() == 11 )
   local dst = ByteArray.init( 99, 88, 77, 66, 55 )
   dst.position = dst.length
   src:readBytes( dst, 3, 4 )
   assert( dst[1] == 99 )
   assert( dst[2] == 88 )
   assert( dst[3] == 77 )
   assert( dst[4] == 22 )
   assert( dst[5] == 33 )
   assert( dst[6] == 44 )
   assert( dst[7] == 55 )
   assert( dst:readByte() == 44 )
   assert( dst:readByte() == 55 )
end

local function test_write_integer()
   local buf = ByteArray.create()
   buf:writeByte( 2 ):writeByte( -2 )
   assert( buf[1] == 2 )
   assert( buf[2] == 254 )
   buf:writeUnsignedByte( 128 )
   assert( buf[3] == 128 )
   buf:writeShort( 27455 ):writeShort( -19999 )
   assert( buf[4] == 27455 % 256 )
   assert( buf[5] == 107 ) -- 27455 / 256
   assert( buf[6] == (0x10000 - 19999) % 256 )
   assert( buf[7] == 177 ) -- (0x10000 - 19999) / 256
   buf:writeUnsignedShort( 46789 )
   assert( buf[8] == 46789 % 256 )
   assert( buf[9] == 182 )
   buf:writeInt( 2334455 ):writeInt( -2334455 )
   assert( buf[10] == 0xf7 )
   assert( buf[11] == 0x9e )
   assert( buf[12] == 0x23 )
   assert( buf[13] == 0x00 )
   assert( buf[14] == 0x09 )
   assert( buf[15] == 0x61 )
   assert( buf[16] == 0xdc )
   assert( buf[17] == 0xff )
   buf:writeUnsignedInt( 3000000000 )
   assert( buf[18] == 0x00 )
   assert( buf[19] == 0x5e )
   assert( buf[20] == 0xd0 )
   assert( buf[21] == 0xb2 )
end

local function test_write_float()
   local buf = ByteArray.create()
   buf:writeFloat( 520.1314 ):writeDouble( 2018.0830 )
   assert( buf[1] == 0x69 )
   assert( buf[2] == 0x08 )
   assert( buf[3] == 0x02 )
   assert( buf[4] == 0x44 )
   assert( buf[5] == 0x46 )
   assert( buf[6] == 0xb6 )
   assert( buf[7] == 0xf3 )
   assert( buf[8] == 0xfd )
   assert( buf[9] == 0x54 )
   assert( buf[10] == 0x88 )
   assert( buf[11] == 0x9f )
   assert( buf[12] == 0x40 )
end

local function test_write_cstr()
   local buf = ByteArray.create()
   buf:writeCString( "hello" )
   buf:writeCString( "world" )
   buf:writeCString( "" )
   buf:writeCString( "a\nb")
   buf.position = 0
   assert( buf:readCString() == "hello" )
   assert( buf:readCString() == "world" )
   assert( buf:readCString() == "" )
   assert( buf:readCString() == "a\nb" )
end

local function test_write_bytes()
   local src = ByteArray.init( 11, 22, 33, 44, 55 )
   local dst = ByteArray.init( 66, 77, 88, 99, 00 );
   src.position = 3;
   src:writeBytes( dst, 2, 2 )
   assert( src[1] == 11 )
   assert( src[2] == 22 )
   assert( src[3] == 33 )
   assert( src[4] == 88 )
   assert( src[5] == 99 )
   assert( src[6] == -1 )
end

local function test_slice()
   local buffer_data = {
      1,2,3,4,5,6,7,8,9,10,
      11,12,13
   }
   local buf = ByteArray.init( buffer_data )
   local a = buf:slice()
   assert( #a == #buffer_data )
   for i=1, #a do
      assert( a[i] == buffer_data[i] )
   end
   local b = buf:slice(4)
   assert( #b == #buffer_data - 4 )
   for i=1, #b do
      assert( b[i] == buf[i+4] )
   end
   local c = buf:slice( 2, 8 )
   assert( #c == 8 - 2 )
   for i=1, #c do
      assert( c[i] == buf[i+2] )
   end
   local d = buf:slice( 6, -3 )
   assert( #d == #buffer_data - 3 - 6 )
   for i=1, #d do
      assert( d[i] == buf[i+6] )
   end
   local e = buf:slice( -7, -2 )
   assert( #e == 7 - 2 )
   for i=1, #e do
      assert( e[i] == buf[i+#buffer_data-7] )
   end
   local f = buf:slice( -1000, -1 )
   assert( #f == #buffer_data - 1 )
   for i=1, #f do
      assert( f[i] == buf[i] )
   end
   local g = buf:slice( -1000, 1000 )
   assert( #g == #buffer_data )
   for i=1, #g do
      assert( g[i] == buf[i] )
   end
end

test_readonly()
test_index()
test_tostring()
test_property_length()
test_property_position()
test_endian()
test_slice()
test_read_integer()
test_read_float()
test_read_cstr()
test_read_bytes()
test_write_integer()
test_write_float()
test_write_cstr()
test_write_bytes()
