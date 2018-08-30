#include "stdio.h"// DEBUG

#include "stdint.h"
#include "stddef.h"
#include "stdlib.h"
#include "string.h"
#include "lauxlib.h"
#include "setjmp.h"
#include "math.h"

#ifndef BYTEARRAY_RESERVE_SIZE
#define BYTEARRAY_RESERVE_SIZE 128		//每个ByteArray在构造的时候预先分配多少内存，单位(字节)
#endif

#define BYTEARRAY_USE_CSTRING

#define ENDIAN_LITTLE 0
#define ENDIAN_BIG 1
#define READ_WRITE 0
#define READ_ONLY 1

typedef struct {
  uint8_t endian: 1;
  uint8_t readonly: 1;
} BufFlag;

enum {
  ERR_OK,
  ERR_NOMEM,
  ERR_OVERFLOW,
  ERR_READONLY,
  ERR_OUTOFRANGE
};

typedef uint32_t buflen_t;

typedef struct {
  uint8_t *buffer;
  BufFlag  flag;
  buflen_t position;
  buflen_t length;
  buflen_t szbuffer;
} Buf;

static jmp_buf except;

static int8_t nativeEndian = -1;
static int getNativeEndian()
{
  if( nativeEndian < 0 )
    {
      int x = 1;
      if( *(char*)&x == 1 ) 
	nativeEndian = ENDIAN_LITTLE;
      else nativeEndian = ENDIAN_BIG;
    }
  return nativeEndian;  
}

static inline BufFlag flag( int endian, int ro )
{
  BufFlag r = {endian, ro};
  return r;
}

static inline buflen_t grow( buflen_t len, buflen_t need )
{
  int times = (len + need) / BYTEARRAY_RESERVE_SIZE;
  return (times+1) * BYTEARRAY_RESERVE_SIZE;
}

#define SWAP(a, b, t) {				\
  t tmp = (a);					\
  (a) = (b);					\
  (b) = tmp;					\
  }

static inline void adjustEndian( uint8_t *first, buflen_t sz, int e )
{
  if( sz > 1 && e != nativeEndian ){
    for(uint8_t *last = &first[sz-1]; first < last; ++first, --last ){
      SWAP( *first, *last, uint8_t );
    }
  }
}

// constructor
static Buf* createBuf( buflen_t sz, int endian )
{
  Buf* retval = realloc( NULL, sizeof(Buf) );
  if( retval == NULL ) return retval;
  
  retval->flag = flag( endian, READ_WRITE );
  retval->position = 0;
  retval->buffer = NULL;
  retval->length = 0;
  retval->szbuffer = 0;
  if( sz > 0 ){
    retval->buffer = realloc( NULL, sz );
    if( retval->buffer == NULL ){
      free( retval );
      return NULL;
    }
    else {
      memset( retval->buffer, 0, sz );
      retval->szbuffer = sz;
    }
  }
  return retval;
}

static Buf* fromArray( void *arr, buflen_t len, int endian )
{
  Buf* retval = realloc( NULL, sizeof(Buf) );
  if( retval == NULL ) return retval;
  
  retval->flag = flag( endian, READ_ONLY );
  retval->position = 0;
  retval->buffer = arr;
  retval->length = len;
  retval->szbuffer = len;
  
  return retval;
}

static void release( Buf *p )
{
  if( !p->flag.readonly )
    free( p->buffer );
  free( p );
}

static inline uint8_t* getBuffer( Buf *p )
{
  return p->buffer;
}

static inline buflen_t getLength( Buf *p )
{
  return p->length;
}

static inline buflen_t getCapacity( Buf *p )
{
  return p->szbuffer;
}

static void resizeBuffer( Buf *p, buflen_t size )
{
  buflen_t l = getCapacity(p);
  if( l == size ) return;

  if( p->flag.readonly ) longjmp( except, ERR_READONLY );
  
  uint8_t *new_buffer = realloc( p->buffer, size );
  if( new_buffer == NULL ) longjmp( except, ERR_NOMEM );
  
  if( l < size ) memset( new_buffer+l, 0, size-l );
  p->buffer = new_buffer;
  p->szbuffer = size;
}

static inline buflen_t getPosition( Buf *p )
{
  return p->position;
}

static inline void setPosition( Buf *p, buflen_t pos )
{
  if( pos < getLength(p) )
    p->position = pos;
  else p->position = getLength(p);
}

static void setLength( Buf *p, buflen_t len )
{
  buflen_t l = getLength(p);
  if( len == l ) return;

  if( p->flag.readonly ) longjmp( except, ERR_READONLY );
  
  buflen_t size = getCapacity(p);
  if( size < len ) 
    resizeBuffer(p, len);

  if( l < len )
    memset( getBuffer(p)+l, 0, len-l );

  p->length = len;

  if( len < getPosition(p) )
    setPosition(p, len);
}

static inline int getEndian( Buf *p )
{
  return p->flag.endian;
}

static inline void setEndian( Buf *p, int e )
{
  p->flag.endian = e;
}

static inline buflen_t getBytesAvailable( Buf *p )
{
  return getLength(p) - getPosition(p);
}

static inline int at( Buf *p, buflen_t pos )
{
  if( pos < getLength(p) )
    return p->buffer[pos];
  else return EOF;
}

static inline void assign( Buf *p, buflen_t pos, uint8_t val )
{
  if( p->flag.readonly ) longjmp( except, ERR_READONLY );
  
  if( getCapacity(p) < pos )
    resizeBuffer(p, pos+1);
  
  getBuffer(p)[ pos++ ] = val;
  p->length = pos > p->length ? pos : p->length;
}

static inline void clear( Buf *p )
{
  if( !p->flag.readonly ) {
    memset( p->buffer, 0, p->szbuffer );
    p->position = 0;
    p->length = 0;
  }
  else longjmp( except, ERR_READONLY );
}

static Buf* cut( Buf *p, size_t pos, size_t len )
{
  size_t size = getLength(p);
  if( pos + len > size ) len = size - pos;

  Buf *retval = createBuf( len, getEndian(p) );
  if( retval == NULL ) longjmp( except, ERR_NOMEM );
  if( pos < size && len > 0 ){
    memcpy( retval->buffer, p->buffer+pos, len );
  }
  
  setLength(retval, len);
  return retval;
}

// ------------ read data ---------------
#define UPDATE_LENGTH(p) p->length = p->length < p->position ? p->position : p->length;

#define RANGE_CHECK( p, sz ) {						\
    if(getBytesAvailable(p) < sz) longjmp( except, ERR_OUTOFRANGE );	\
  }

static int readBoolean( Buf *p )
{
  RANGE_CHECK( p, sizeof(uint8_t) );

  return at(p, p->position++) != 0;
}

static void readBytes( Buf *p, void *bytes, uint32_t offset, size_t length )
{
  RANGE_CHECK( p, length );

  uint8_t *p_data_src = bytes;
  p_data_src += offset;

  memcpy( p_data_src, p->buffer + p->position, length );
  p->position += length;
}

#define READ_BUILDIN_TEMPLATE( type, name )				\
  static type name( Buf *p )						\
  {									\
    size_t sz = sizeof(type);						\
    RANGE_CHECK(p, sz);							\
									\
    type retval = *(type*)(p->buffer + p->position);			\
    adjustEndian( (uint8_t*)&retval, sz, getEndian(p) );		\
    									\
    p->position += sz;							\
    return retval;							\
  }

READ_BUILDIN_TEMPLATE( uint8_t, readUnsignedByte )
READ_BUILDIN_TEMPLATE( int8_t, readByte )
READ_BUILDIN_TEMPLATE( uint16_t, readUnsignedShort )
READ_BUILDIN_TEMPLATE( int16_t, readShort )
READ_BUILDIN_TEMPLATE( uint32_t, readUnsignedInt )
READ_BUILDIN_TEMPLATE( int32_t, readInt )
READ_BUILDIN_TEMPLATE( double, readDouble )
READ_BUILDIN_TEMPLATE( float, readFloat )

// ------------ write data ---------------

#define RANGE_RESERVE( p, sz ) {				\
    if( p->flag.readonly ) longjmp( except, ERR_READONLY );	\
								\
    if( getCapacity(p) - getPosition(p) < sz ){			\
      buflen_t nsz = grow(getCapacity(p), sz);			\
      if( nsz < getCapacity(p) ) longjmp(except, ERR_OVERFLOW);	\
								\
      resizeBuffer(p, nsz);					\
    }								\
  }

static void writeBoolean( Buf *p, int value )
{
  RANGE_RESERVE(p, 1);

  uint8_t boolean = value != 0;
  getBuffer(p)[p->position++] = boolean;
  UPDATE_LENGTH(p);
}

static void writeBytes( Buf *p, const void *bytes, uint32_t offset, size_t length )
{
  RANGE_RESERVE(p, length);

  const uint8_t *src = (uint8_t*)bytes + offset;
  memcpy( p->buffer + p->position, src, length );
  p->position += length;
  UPDATE_LENGTH(p);
}

#define WRITE_BUILDIN_TEMPLATE( type, name )		\
  static void name( Buf *p, type value )		\
  {							\
    size_t sz = sizeof(type);				\
    RANGE_RESERVE(p, sz);				\
							\
    type *pvalue = (type*)(p->buffer + p->position);	\
    *pvalue = value;					\
    adjustEndian((uint8_t*)pvalue, sz, getEndian(p));	\
							\
    p->position += sz;					\
    UPDATE_LENGTH(p);					\
  }

WRITE_BUILDIN_TEMPLATE( uint8_t, writeUnsignedByte )
WRITE_BUILDIN_TEMPLATE( int8_t, writeByte )
WRITE_BUILDIN_TEMPLATE( uint16_t, writeUnsignedShort )
WRITE_BUILDIN_TEMPLATE( int16_t, writeShort )
WRITE_BUILDIN_TEMPLATE( uint32_t, writeUnsignedInt )
WRITE_BUILDIN_TEMPLATE( int32_t, writeInt )
WRITE_BUILDIN_TEMPLATE( double, writeDouble )
WRITE_BUILDIN_TEMPLATE( float, writeFloat )

// ------------------- for lua -------------------

// -------------- literal constant in lvm ----------------

#ifdef _MEMORY_ECONOMY
// declare lua_error message content
#define MSG_NOMEM                      "NoMem"
#define MSG_OUTOFRANGE                 "EndOfBuf"
#define MSG_OVERFLOW                   "LenOvfl"
#define MSG_INVALIDTYPE                "ErrorType"
#define MSG_READONLY                   "RoBuf"

// declare name for module
#define MODULE_NAME                    "buf"

#define CONSTANT_ENDIAN_L              "LE"
#define CONSTANT_ENDIAN_B              "BE"

// declare constructor
#define CONSTRUCTOR_CREATE             "create" // local b = buf.create(size, endian)
#define CONSTRUCTOR_INITER             "init"   // local b = buf.init( 49, 50, 51 )
#define CONSTRUCTOR_FROMARR            "load"   // local b = buf.load( "hello,world" ) -- read only

// declare member
#define MEMBER_LENGTH                  "len"    // local l = b.length OR b.length = 1024
#define MEMBER_POSITION                "pos"    // local p = b.pos OR b.pos = 1
#define MEMBER_ENDIAN                  "endian" // local e = b.endian OR b.endian = 0
#define MEMBER_AVAILABLE               "free"   // local a = b.free                    -- read only

// declare method
#define METHOD_READBOOL                "rdb"    // local t = b:rdb()
#define METHOD_WRITEBOOL               "wrb"    // b:wrb( false )
#define METHOD_READU8                  "u8r"    // local u = b:u8r()
#define METHOD_WRITEU8                 "u8w"    // b:u8w( 0x21 )
#define METHOD_READS8                  "s8r"    // local i = b:s8r()
#define METHOD_WRITES8                 "s8w"    // b:s8w( 49 )
#define METHOD_READU16                 "u16r"
#define METHOD_WRITEU16                "u16w"
#define METHOD_READS16                 "s16r"
#define METHOD_WRITES16                "s16w"
#define METHOD_READU32                 "u32r"
#define METHOD_WRITEU32                "u32w"
#define METHOD_READS32                 "s32r"
#define METHOD_WRITES32                "s32w"
#define METHOD_READFLOAT               "f32r"
#define METHOD_WRITEFLOAT              "f32w"
#define METHOD_READDOUBLE              "f64r"
#define METHOD_WRITEDOUBLE             "f64w"
#define METHOD_READCSTR                "strr"
#define METHOD_WRITECSTR               "strw"

#define METHOD_READBYTES               "read"   // local s = buf.create(); b:read(s, 0, b.length)
#define METHOD_WRITEBYTES              "write"  // local s = buf.load("hello"); b:write(s, 0, s.length)

#define METHOD_CUT                     "cut"    // local t = buf.load("hello,world"):cut( 6, 11 )
#define METHOD_CLEAR                   "clear"  // b:clear()
#define METHOD_TOSTRING                "str"    // b:str()
#else
// declare lua_error message content
#define MSG_NOMEM                      "memory not enough"
#define MSG_OUTOFRANGE                 "out of buffer range"
#define MSG_OVERFLOW                   "buffer size overflow"
#define MSG_INVALIDTYPE                "invalid type"
#define MSG_READONLY                   "buffer is readonly"

// declare name for module
#define MODULE_NAME                    "ByteArray"

#define CONSTANT_ENDIAN_L              "LITTLE_ENDIAN"
#define CONSTANT_ENDIAN_B              "BIG_ENDIAN"

// declare constructor
#define CONSTRUCTOR_CREATE             "create"
#define CONSTRUCTOR_INITER             "init"  
#define CONSTRUCTOR_FROMARR            "load"  

// declare member
#define MEMBER_LENGTH                  "length"
#define MEMBER_POSITION                "position"   
#define MEMBER_ENDIAN                  "endian"
#define MEMBER_AVAILABLE               "bytesAvailable"

// declare method
#define METHOD_READBOOL                "readBoolean"
#define METHOD_WRITEBOOL               "writeBoolean"
#define METHOD_READU8                  "readUnsignedByte"
#define METHOD_WRITEU8                 "writeUnsignedByte"
#define METHOD_READS8                  "readByte"
#define METHOD_WRITES8                 "writeByte"
#define METHOD_READU16                 "readUnsignedShort"
#define METHOD_WRITEU16                "writeUnsignedShort"
#define METHOD_READS16                 "readShort"
#define METHOD_WRITES16                "writeShort"
#define METHOD_READU32                 "readUnsignedInt"
#define METHOD_WRITEU32                "writeUnsignedInt"
#define METHOD_READS32                 "readInt"
#define METHOD_WRITES32                "writeInt"
#define METHOD_READFLOAT               "readFloat"
#define METHOD_WRITEFLOAT              "writeFloat"
#define METHOD_READDOUBLE              "readDouble"
#define METHOD_WRITEDOUBLE             "writeDouble"
#define METHOD_READCSTR                "readCString"
#define METHOD_WRITECSTR               "writeCString"

#define METHOD_READBYTES               "readBytes"
#define METHOD_WRITEBYTES              "writeBytes"

#define METHOD_CUT                     "slice"
#define METHOD_CLEAR                   "clear"
#define METHOD_TOSTRING                "toString"
#endif

#define new_buffer( p, sz, e ) {		\
    p = createBuf( sz, e );			\
    if( !p ){					\
      error_handle(L, ERR_NOMEM);		\
      lua_error(L);				\
      return 0;					\
    }						\
  }

#define check_userdata_self( L )		\
  if( !lua_islightuserdata(L, 1) ){		\
    luaL_argerror(L, 1, MSG_INVALIDTYPE);	\
    return 0;					\
  }

#define set_bytearr_metatable( L ) {				\
    lua_getfield( L, LUA_REGISTRYINDEX, MODULE_NAME "#mt" );	\
    lua_setmetatable( L, -2 );					\
  }

void error_handle( lua_State *L, int errno )
{
  if( errno == ERR_NOMEM ){
    lua_pushstring( L, MSG_NOMEM );
  }
  else if( errno == ERR_OVERFLOW ){
    lua_pushstring( L, MSG_OVERFLOW );
  }
  else if( errno == ERR_READONLY ){
    lua_pushstring( L, MSG_READONLY );
  }
  else if( errno == ERR_OUTOFRANGE ){
    lua_pushstring( L, MSG_OUTOFRANGE );
  }
}

// buf.create( [size, endian] )
static int lbytearr_create( lua_State *L )
{
  int size = BYTEARRAY_RESERVE_SIZE;
  if( lua_isnumber( L, 1 ) ) {
    size = luaL_checkint( L, 1 );
  }
  
  int endian = getNativeEndian();
  if( lua_isnumber( L, 2 ) ) {
    endian = luaL_checkint( L, 2 );
  }
  
  Buf* retval;
  new_buffer( retval, size, endian );
  
  lua_pushlightuserdata( L, retval );
  set_bytearr_metatable( L );
  return 1;
}

// buf.init( 1,2,3 ) 或者 buf.init( {1,2,3} )
static int lbytearr_init( lua_State *L )
{
  int n = lua_gettop( L );
  if( n < 1 ) {
    luaL_argerror(L, 1, MSG_INVALIDTYPE);
    return 0;
  }
  
  Buf *retval = NULL;
  int endian = getNativeEndian();
  
  if(lua_isnumber(L, 1)){
    new_buffer( retval, n, endian );
    
    for( int i=1; i <= n; ++i ){
      if( !lua_isnumber(L, i) ){
	release( retval );
	luaL_argerror(L, i, MSG_INVALIDTYPE);
	
	return 0;
      }

      getBuffer(retval)[i-1] = (uint8_t)lua_tointeger(L, i);
    }
    retval->length = n;
  }
  else {
    buflen_t max = ~0;
    size_t total = 0;
    for( int i=1; i <= n; ++i ){
      luaL_checktype(L, i, LUA_TTABLE);
      total += lua_objlen(L, i);
    }

    if( total > (size_t) max ){
      error_handle(L, ERR_OVERFLOW);
      lua_error(L);
      return 0;
    }

    new_buffer( retval, total, endian );
    for( int i=1; i <= n; ++i ){
      size_t len = lua_objlen( L, i );
      for( int j=1; j <= len; ++j ){
	lua_pushinteger(L, j);
	lua_gettable(L, i);
	if( !lua_isnumber(L, -1) ){
	  release(retval);
	  luaL_argerror( L, i, MSG_INVALIDTYPE );
	  return 0;
	}

	writeUnsignedByte(retval, lua_tointeger(L, -1));
	lua_pop(L, 1);
      }// end of j
    }// end of i
    
    // rewind the cursor
    setPosition(retval, 0);
  }

  lua_pushlightuserdata(L, retval);
  set_bytearr_metatable(L);
  return 1;
}

// local buf = ByteArray.load("hello,world")
static int lbytearr_load( lua_State *L )
{
  luaL_checkstring(L, 1);

  size_t sz;
  const char *p = lua_tolstring(L, 1, &sz);
  int endian = getNativeEndian();
  if( lua_isnumber(L, 2) ){
    endian = lua_tonumber( L, 2 );
  }

  Buf *retval = fromArray( (void*)p, sz, endian );
  if( !retval ){
    error_handle(L, ERR_NOMEM);
    lua_error(L);
    return 0;
  }
  
  lua_pushlightuserdata( L, retval );
  set_bytearr_metatable( L );
  
  return 1;
}

// local str = buf:toString()
static int lbytearr_tostring( lua_State *L )
{
  check_userdata_self(L);
  
  Buf *p = lua_touserdata(L, 1);
  char *b = (char*)getBuffer(p);
  size_t len = getLength(p);
  lua_pushlstring(L, b, len);
  return 1;
}

#define handle_scope_except()			\
  int err = setjmp( except );			\
  if( err ){					\
    error_handle( L, err );			\
    lua_error(L);				\
    return 0;					\
  }

#define bytes_param_check(p, s, offset, length) {	\
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);		\
    luaL_checktype(L, 2, LUA_TLIGHTUSERDATA);		\
    p = lua_touserdata(L, 1);				\
    s = lua_touserdata(L, 2);				\
    buflen_t max = ~0;					\
    offset = 0;						\
    length = 0;						\
    int n = lua_gettop(L);				\
    if( n >= 3 ){					\
      offset = luaL_checkint(L, 3);			\
    }							\
    if( n >= 4 ) {					\
      length = luaL_checkint(L, 4);			\
    }							\
    if( offset > (size_t)max || length > (size_t)max ){	\
      error_handle(L, ERR_OVERFLOW);			\
      lua_error(L);					\
      return 0;						\
    }							\
  }							

// local buffer = ByteArray.load("hello,world")
// local s = ByteArray.create()
// buffer:readBytes( s, 0, buffer.bytesAvailable )
static int lbytearr_readbytes( lua_State *L )
{
  Buf *p, *bytes;
  size_t offset, length;
  
  bytes_param_check(p, bytes, offset, length);

  handle_scope_except();

  if( length == 0 ) length = getBytesAvailable(p);
  if( getLength(bytes) < offset + length ) {
    setLength(bytes, offset+length);
  }
    
  readBytes(p, getBuffer(bytes), offset, length);
  return 0;
}

// local buffer = ByteArray.load("hello,world")
// local s = ByteArray.create()
// s:writeBytes( buffer, 6, 5 )
static int lbytearr_writebytes( lua_State *L )
{
  Buf *p, *bytes;
  size_t offset, length;

  bytes_param_check(p, bytes, offset, length);

  handle_scope_except();

  if( length == 0 ) length = getLength(bytes) - offset;
  if( getLength(bytes) < offset + length ){
    length = getLength(bytes) - offset;
  }
  
  writeBytes( p, getBuffer(bytes), offset, length );
  return 0;
}

#define LUA_BIND_BUILDIN_WRITER( NAME, FUNC, CHECKF, TYPE )	\
  static int lbytearr_##NAME( lua_State *L )			\
  {								\
    check_userdata_self(L);					\
								\
    Buf *p = lua_touserdata(L, 1);				\
    TYPE b = CHECKF(L, 2);					\
								\
    handle_scope_except();					\
    								\
    FUNC(p, b);							\
    lua_pushlightuserdata(L, p);				\
    return 1;							\
  }

LUA_BIND_BUILDIN_WRITER( writebool, writeBoolean, lua_toboolean, int );
LUA_BIND_BUILDIN_WRITER( writes8, writeByte, luaL_checknumber, int );
LUA_BIND_BUILDIN_WRITER( writeu8, writeUnsignedByte, luaL_checknumber, int );
LUA_BIND_BUILDIN_WRITER( writes16, writeShort, luaL_checknumber, int );
LUA_BIND_BUILDIN_WRITER( writeu16, writeUnsignedShort, luaL_checknumber, int );
LUA_BIND_BUILDIN_WRITER( writes32, writeInt, luaL_checknumber, int );
LUA_BIND_BUILDIN_WRITER( writeu32, writeUnsignedInt, luaL_checknumber, uint32_t );
LUA_BIND_BUILDIN_WRITER( writef32, writeFloat, luaL_checknumber, float );
LUA_BIND_BUILDIN_WRITER( writef64, writeDouble, luaL_checknumber, double );

#define LUA_BIND_BUILDIN_READER( NAME, FUNC, TYPE, PUSHF ) \
  static int lbytearr_##NAME( lua_State *L )		   \
  {							   \
    check_userdata_self(L);				   \
							   \
    Buf *p = lua_touserdata(L, 1);			   \
							   \
    handle_scope_except();				   \
							   \
    TYPE retval = FUNC(p);				   \
    lua_##PUSHF(L, retval);				   \
    return 1;						   \
  }

LUA_BIND_BUILDIN_READER( readbool, readBoolean, int, pushboolean );
LUA_BIND_BUILDIN_READER( reads8, readByte, int, pushinteger );
LUA_BIND_BUILDIN_READER( readu8, readUnsignedByte, int, pushinteger );
LUA_BIND_BUILDIN_READER( reads16, readShort, int, pushinteger );
LUA_BIND_BUILDIN_READER( readu16, readUnsignedShort, int, pushinteger );
LUA_BIND_BUILDIN_READER( reads32, readInt, int, pushinteger );
LUA_BIND_BUILDIN_READER( readu32, readUnsignedInt, uint32_t, pushnumber );
LUA_BIND_BUILDIN_READER( readf32, readFloat, float, pushnumber );
LUA_BIND_BUILDIN_READER( readf64, readDouble, double, pushnumber );

#ifdef BYTEARRAY_USE_CSTRING
// local t = b:readCString()
static int lbytearr_readstr( lua_State *L )
{
  check_userdata_self(L);
  
  Buf *p = lua_touserdata(L, 1);
  char *str = (char*)&getBuffer(p)[getPosition(p)];
  
  size_t l = 1 + strlen( str );
  if( getBytesAvailable(p) < l ){
    error_handle(L, ERR_OUTOFRANGE);
    lua_error(L);
    return 0;
  }
  
  lua_pushstring(L, str);
  p->position += l;
  return 1;
}
static int lbytearr_writestr( lua_State *L )
{
  check_userdata_self(L);

  luaL_checktype(L, 2, LUA_TSTRING);
  
  Buf *p = lua_touserdata(L, 1);
  const char *pstr = lua_tostring(L, 2);
  uint8_t *str = &getBuffer(p)[getPosition(p)];
  
  size_t l = 1 + lua_objlen(L, 2);
  
  handle_scope_except();
  
  RANGE_RESERVE( p, l );
  
  memcpy( str, pstr, l-1 );
  str[l-1] = '\0';
  p->position += l;
  p->length += l;

  lua_pushlightuserdata(L, p);
  return 1;
}
#endif//BYTEARRAY_USE_CSTRING

static int lbytearr_clear( lua_State *L )
{
  check_userdata_self(L);

  handle_scope_except();
  
  Buf *p = lua_touserdata(L, 1);
  clear( p );
  return 1;
}

static int lbytearr_slice( lua_State *L )
{
  check_userdata_self(L);

  Buf *p = lua_touserdata(L, 1);
  
  int start = 0;
  int end = getLength(p);
  size_t n = lua_gettop( L );
  if( n > 1 ){
    start = luaL_checkint(L, 2);
  }
  if( n > 2 ){
    end = luaL_checkint(L, 3);
  }
  if( start < 0 ){
    start += getLength(p);
  }
  if( end <= 0 ){
    end += getLength(p);
  }
  if( start > end || getLength(p) <= start ){
    error_handle(L, ERR_OUTOFRANGE);
    lua_error(L);
    return 0;
  }
  
  handle_scope_except();
    
  Buf *r = cut(p, start, end-start);
  lua_pushlightuserdata(L, r);
  return 1;
}

static luaL_Reg bytearr_map[] = {
  { CONSTRUCTOR_CREATE, lbytearr_create },
  { CONSTRUCTOR_INITER, lbytearr_init },
  { CONSTRUCTOR_FROMARR, lbytearr_load },

  { METHOD_TOSTRING, lbytearr_tostring },
  { METHOD_CLEAR, lbytearr_clear },
  { METHOD_CUT, lbytearr_slice },

  { METHOD_WRITEBOOL, lbytearr_writebool },
  { METHOD_WRITEU8, lbytearr_writeu8 },
  { METHOD_WRITES8, lbytearr_writes8 },
  { METHOD_WRITEU16, lbytearr_writeu16 },
  { METHOD_WRITES16, lbytearr_writes16 },
  { METHOD_WRITEU32, lbytearr_writeu32 },
  { METHOD_WRITES32, lbytearr_writes32 },
  { METHOD_WRITEFLOAT, lbytearr_writef32 },
  { METHOD_WRITEDOUBLE, lbytearr_writef64 },
  { METHOD_READBOOL, lbytearr_readbool },
  { METHOD_READU8, lbytearr_readu8 },
  { METHOD_READS8, lbytearr_reads8 },
  { METHOD_READU16, lbytearr_readu16 },
  { METHOD_READS16, lbytearr_reads16 },
  { METHOD_READU32, lbytearr_readu32 },
  { METHOD_READS32, lbytearr_reads32 },
  { METHOD_READFLOAT, lbytearr_readf32 },
  { METHOD_READDOUBLE, lbytearr_readf64 },
  { METHOD_READBYTES, lbytearr_readbytes },
  { METHOD_WRITEBYTES, lbytearr_writebytes },
#ifdef BYTEARRAY_USE_CSTRING
  { METHOD_READCSTR, lbytearr_readstr },
  { METHOD_WRITECSTR, lbytearr_writestr },
#endif
  {NULL, NULL}
};

static int lbytearr_getlen( lua_State *L )
{
  check_userdata_self(L);

  Buf *p = lua_touserdata(L, 1);
  lua_pushinteger( L, getLength(p) );
  return 1;
}

static int lbytearr_getter( lua_State *L )
{
  check_userdata_self(L);

  Buf *p = lua_touserdata(L, 1);
  
  if( lua_type(L, 2) == LUA_TNUMBER ){
    lua_Number arg2 = lua_tonumber(L, 2);
    buflen_t idx = (buflen_t)arg2;
    if( fabs( (float)arg2 - (float)idx ) < 0.00001f )
      lua_pushinteger( L, at(p, lua_tointeger(L, 2)-1) );
    else lua_pushinteger(L, -1);
  }
  else if( lua_isstring(L, 2) ){
    const char *key = lua_tostring(L, 2);
    // if exist in MODULE
    lua_getglobal(L, MODULE_NAME);
    lua_getfield(L, -1, key);
    
    if( lua_isnil(L, -1) ){
      //if the member name
      if( 0 == strcmp(key, MEMBER_LENGTH) ){
	lua_pushinteger(L, getLength(p));
      }
      else if( 0 == strcmp(key, MEMBER_POSITION) ){
	lua_pushinteger(L, getPosition(p));
      }
      else if( 0 == strcmp(key, MEMBER_AVAILABLE) ){
	lua_pushinteger(L, getBytesAvailable(p));
      }
      else if( 0 == strcmp(key, MEMBER_ENDIAN) ){
	lua_pushinteger(L, getEndian(p));
      }
    }
  }
  else lua_pushnil(L);
  
  return 1;
}

static int lbytearr_setter( lua_State *L )
{
  check_userdata_self(L);

  Buf *p = lua_touserdata(L, 1);
  
  handle_scope_except();
  
  int val;
  val = luaL_checkint(L, 3);
  if( lua_isnumber(L, 2) ){
    int index = lua_tointeger(L, 2) - 1;
    luaL_argcheck(L, 0 <= index, 2, MSG_OUTOFRANGE);
    assign( p, index, (uint8_t)(val & 0xff) );
  }
  else if( lua_isstring(L, 2) ){
    const char *key = lua_tostring(L, 2);
    
    //if the member name
    if( 0 == strcmp(key, MEMBER_LENGTH) ){
      if( val < 0 ) longjmp( except, ERR_OUTOFRANGE );
      setLength(p, val);
    }
    else if( 0 == strcmp(key, MEMBER_POSITION) ){
      if( val < 0 ) longjmp( except, ERR_OUTOFRANGE );
      setPosition(p, val);
    }
    else if( 0 == strcmp(key, MEMBER_ENDIAN) ){
      setEndian(p, val);
    }
    else longjmp( except, ERR_OUTOFRANGE );
  }
  else longjmp( except, ERR_OUTOFRANGE );
  
  return 0;
}

static int lbytearr_gc( lua_State *L )
{
  check_userdata_self(L);

  Buf *p = lua_touserdata(L, 1);
  release( p );
  return 0;
}

int luaopen_bytearr( lua_State *L )
{
  luaL_register(L, MODULE_NAME, bytearr_map );

  lua_pushinteger(L, ENDIAN_LITTLE);
  lua_setfield(L, -2, CONSTANT_ENDIAN_L );

  lua_pushinteger(L, ENDIAN_BIG);
  lua_setfield(L, -2, CONSTANT_ENDIAN_B );
  
  // metatable
  lua_newtable(L);

  lua_pushcfunction(L, lbytearr_getlen);
  lua_setfield(L, -2, "__len");
  
  lua_pushcfunction(L, lbytearr_getter);
  lua_setfield(L, -2, "__index");

  lua_pushcfunction(L, lbytearr_setter);
  lua_setfield(L, -2, "__newindex");

  lua_pushcfunction(L, lbytearr_gc);
  lua_setfield(L, -2, "__gc");

  //lua_pushcfunction(L, lbytearr_add);
  //lua_pushstring(L, "__add");
  lua_setfield(L, LUA_REGISTRYINDEX, MODULE_NAME "#mt");
  
  return 0;
}
