#include "cpgf/scriptbind/gluabind.h"
#include "cpgf/gflags.h"
#include "cpgf/gexception.h"
#include "cpgf/gmetaclasstraveller.h"
#include "cpgf/gcallback.h"

//#include "../../pinclude/gbindcommon.h"
#include "gbindcommon_new.h"
#include "../../pinclude/gscriptbindapiimpl.h"


#include <stdexcept>
#include <algorithm>
#include <vector>

#include <string.h>


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996)
#endif


#if LUA_VERSION_NUM >= 502
	#define HAS_LUA_GLOBALSINDEX 0
#else
	#define HAS_LUA_GLOBALSINDEX 1
#endif


using namespace std;
using namespace cpgf::_bind_internal;

#define ENTER_LUA() \
	char local_msg[256]; bool local_error = false; { \
	try {

#define LEAVE_LUA(L, ...) \
	} \
	catch(const GException & e) { strncpy(local_msg, e.getMessage(), 256); local_error = true; } \
	catch(...) { strcpy(local_msg, "Unknown exception occurred."); local_error = true; } \
	} if(local_error) { local_msg[255] = 0; error(L, local_msg); } \
	__VA_ARGS__;
	

namespace cpgf {


namespace {

class GLuaScriptObject;
class GLuaGlobalAccessor;

class GLuaScriptFunction : public GScriptFunction
{
public:
	GLuaScriptFunction(const GContextPointer & context, int objectIndex);
	virtual ~GLuaScriptFunction();
	
	virtual GMetaVariant invoke(const GMetaVariant * params, size_t paramCount);
	virtual GMetaVariant invokeIndirectly(GMetaVariant const * const * params, size_t paramCount);

private:
	GWeakContextPointer context;
	int ref;
};

class GLuaScriptObject : public GScriptObject
{
private:
	typedef GScriptObject super;

public:
	GLuaScriptObject(IMetaService * service, lua_State * L, const GScriptConfig & config);
	GLuaScriptObject(const GLuaScriptObject & other);
	GLuaScriptObject(IMetaService * service, lua_State * L, const GScriptConfig & config, int objectIndex);
	virtual ~GLuaScriptObject();

	virtual bool isGlobal() const;
	
	virtual void bindClass(const char * name, IMetaClass * metaClass);
	virtual void bindEnum(const char * name, IMetaEnum * metaEnum);

	virtual void bindFundamental(const char * name, const GVariant & value);
	virtual void bindAccessible(const char * name, void * instance, IMetaAccessible * accessible);
	virtual void bindString(const char * stringName, const char * s);
	virtual void bindObject(const char * objectName, void * instance, IMetaClass * type, bool transferOwnership);
	virtual void bindRaw(const char * name, const GVariant & value);
	virtual void bindMethod(const char * name, void * instance, IMetaMethod * method);
	virtual void bindMethodList(const char * name, IMetaList * methodList);

	virtual IMetaClass * getClass(const char * className);
	virtual IMetaEnum * getEnum(const char * enumName);

	virtual GVariant getFundamental(const char * name);
	virtual std::string getString(const char * stringName);
	virtual void * getObject(const char * objectName);
	virtual GVariant getRaw(const char * name);
	virtual IMetaMethod * getMethod(const char * methodName, void ** outInstance);
	virtual IMetaList * getMethodList(const char * methodName);
	
	virtual GScriptDataType getType(const char * name, IMetaTypedItem ** outMetaTypeItem);

	virtual GScriptObject * createScriptObject(const char * name);
	virtual GScriptObject * gainScriptObject(const char * name);
	
	virtual GScriptFunction * gainScriptFunction(const char * name);
	
	virtual GMetaVariant invoke(const char * name, const GMetaVariant * params, size_t paramCount);
	virtual GMetaVariant invokeIndirectly(const char * name, GMetaVariant const * const * params, size_t paramCount);

	virtual void assignValue(const char * fromName, const char * toName);
	virtual bool valueIsNull(const char * name);
	virtual void nullifyValue(const char * name);

	virtual void bindCoreService(const char * name);

public:
	const GContextPointer & getContext() const {
		return this->context;
	}
	
	lua_State * getLuaState() const {
		return this->luaState;
	}

private:
	void getTable() const;
	GMethodGlueDataPointer doGetMethodUserData();
	GLuaGlobalAccessor * getGlobalAccessor();


private:
	GContextPointer context;
	GLuaScriptObject * binding;
	lua_State * luaState;
	int ref;
	GScopedPointer<GLuaGlobalAccessor> globalAccessor;

private:
	friend class GLuaScopeGuard;
	friend class GLuaScriptObjectImplement;
	friend class GLuaScriptNameData;
	friend class GLuaGlobalAccessor;
};


class GLuaBindingContext : public GBindingContext, public GShareFromBase
{
private:
	typedef GBindingContext super;

public:
	GLuaBindingContext(IMetaService * service, const GScriptConfig & config, lua_State * luaState)
		: super(service, config), luaState(luaState)
	{
	}

	lua_State * getLuaState() const {
		return this->luaState;
	}

private:
	lua_State * luaState;
};


class GLuaScopeGuard
{
public:
	GLuaScopeGuard(GScriptObject * scope);
	~GLuaScopeGuard();
	
	void keepStack();

	void set(const char * name);
	void get(const char * name);
	void rawGet(const char * name);

private:
	GLuaScriptObject * scope;
	int top;
};


class GLuaGlobalAccessorItem
{
public:
	GLuaGlobalAccessorItem(void * instance, IMetaAccessible * accessible) : instance(instance), accessible(accessible) {
	}
	
public:
	void * instance;
	GSharedInterface<IMetaAccessible> accessible;
};

class GLuaGlobalAccessor
{
private:
	typedef map<std::string, GLuaGlobalAccessorItem> MapType;

public:
	explicit GLuaGlobalAccessor(GLuaScriptObject * scriptObject);
	void doBindAccessible(const char * name, void * instance, IMetaAccessible * accessible);

	int doIndex();
	int doNewIndex();

private:
	void initialize();

	int doPreviousIndex(const char * name);
	int doPreviousNewIndex(const char * name);

private:
	MapType itemMap;
	GLuaScriptObject * scriptObject;
};

int UserData_gc(lua_State * L);
int UserData_call(lua_State * L);
int UserData_index(lua_State * L);
int UserData_newindex(lua_State * L);
int UserData_operator(lua_State * L);

void doBindAllOperators(const GContextPointer & context, void * instance, IMetaClass * metaClass);
void doBindClass(const GContextPointer & context, IMetaClass * metaClass);
void doBindEnum(const GContextPointer & context, IMetaEnum * metaEnum);
//void doBindMethodList(const GContextPointer & context, const GSharedObjectData & objectData, const GMethodData & data);
//void doBindMethodList(const GContextPointer & context, const char * name, IMetaList * methodList, GUserDataMethodType methodType);

void initObjectMetaTable(lua_State * L);
void setMetaTableGC(lua_State * L);
void setMetaTableCall(lua_State * L, void * userData);
void setMetaTableSignature(lua_State * L);
bool isValidMetaTable(lua_State * L, int index);

bool variantToLua(const GContextPointer & context, const GVariant & value, const GMetaType & type, bool allowGC, bool allowRaw);
GMetaVariant luaToVariant(const GContextPointer & context, int index);

bool doIndexMemberData(const GContextPointer & context, IMetaAccessible * data, void * instance, bool instanceIsConst);

void error(lua_State * L, const char * message);

const int RefTable = LUA_REGISTRYINDEX;

int refLua(lua_State * L, int objectIndex)
{
	lua_pushvalue(L, objectIndex);
	return luaL_ref(L, RefTable);
}

void unrefLua(lua_State * L, int ref)
{
	if(ref == LUA_NOREF || L == NULL) {
		return;
	}

	luaL_unref(L, RefTable, ref);
}

void getRefObject(lua_State * L, int ref)
{
	if(ref == LUA_NOREF) {
		return;
	}

	lua_rawgeti(L, RefTable, ref);
}

lua_State * getLuaState(const GContextPointer & context)
{
	if(! context) {
		return NULL;
	}
	else {
		return gdynamic_cast<GLuaBindingContext *>(context.get())->getLuaState();
	}
}


/*
void dumpLuaValue(lua_State * L, int index)
{
	int type = lua_type(L, index);

	cout << "XXX ";
	switch(type) {
		case LUA_TNIL:
			cout << "nil";
			break;

		case LUA_TNUMBER:
			cout << "Number " << lua_tonumber(L, index);
			break;

		case LUA_TBOOLEAN:
			cout << "Boolean " << lua_toboolean(L, index);
			break;

		case LUA_TSTRING:
			cout << "String " << lua_tostring(L, index);
			break;

		case LUA_TUSERDATA:
			cout << "Userdata " << lua_touserdata(L, index);
			break;

		case LUA_TFUNCTION:
			cout << "Function";
			break;

		case LUA_TLIGHTUSERDATA:
			break;

		case LUA_TTABLE:
			cout << "Table" << lua_topointer(L, index);
			break;
			
		case LUA_TTHREAD:
			break;
	}

	cout << endl;
}
*/

int GlobalAccessor_index(lua_State * L)
{
	ENTER_LUA()

	GLuaGlobalAccessor * accessor = static_cast<GLuaGlobalAccessor *>(lua_touserdata(L, lua_upvalueindex(1)));

	return accessor->doIndex();

	LEAVE_LUA(L, lua_pushnil(L); return 0)
}

int GlobalAccessor_newindex(lua_State * L)
{
	ENTER_LUA()

	GLuaGlobalAccessor * accessor = static_cast<GLuaGlobalAccessor *>(lua_touserdata(L, lua_upvalueindex(1)));
	
	return accessor->doNewIndex();
	
	LEAVE_LUA(L, return 0)
}

GLuaGlobalAccessor::GLuaGlobalAccessor(GLuaScriptObject * scriptObject)
	: scriptObject(scriptObject)
{
	this->initialize();
}

void GLuaGlobalAccessor::doBindAccessible(const char * name, void * instance, IMetaAccessible * accessible)
{
	string sname(name);

	this->itemMap.insert(make_pair(sname, GLuaGlobalAccessorItem(instance, accessible)));
}

int GLuaGlobalAccessor::doIndex()
{
	lua_State * L = this->scriptObject->getLuaState();

	const char * name = lua_tostring(L, -1);

	string sname(name);
	MapType::iterator it = this->itemMap.find(sname);
	if(it != this->itemMap.end()) {
		doIndexMemberData(this->scriptObject->getContext(), it->second.accessible.get(), it->second.instance, false);
		return 1;
	}

	return this->doPreviousIndex(name);
}

int GLuaGlobalAccessor::doNewIndex()
{
	lua_State * L = this->scriptObject->getLuaState();

	const char * name = lua_tostring(L, -2);
	
	string sname(name);
	MapType::iterator it = this->itemMap.find(sname);
	if(it != this->itemMap.end()) {
		GVariant value = luaToVariant(this->scriptObject->getContext(), -1).getValue();
		GVariantData varData = value.getData();
		it->second.accessible->set(it->second.instance, &varData);
		metaCheckError(it->second.accessible);
		return 1;
	}

	return this->doPreviousNewIndex(name);
}

int GLuaGlobalAccessor::doPreviousIndex(const char * name)
{
	lua_State * L = this->scriptObject->getLuaState();
	
	this->scriptObject->getTable();

	if(lua_isfunction(L, -1)) {
		lua_pushstring(L, name);
		lua_call(L, 1, 1);
		return 1;
	}
	if(lua_istable(L, -1)) {
		lua_pushstring(L, name);
		lua_rawget(L, -2);

		return 1;
	}

	return 0;
}

int GLuaGlobalAccessor::doPreviousNewIndex(const char * name)
{
	lua_State * L = this->scriptObject->getLuaState();
	
	this->scriptObject->getTable();

	if(lua_isfunction(L, -1)) {
		lua_pushstring(L, name);
		lua_pushvalue(L, -3);
		lua_call(L, 2, 0);
		return 1;
	}
	if(lua_istable(L, -1)) {
		lua_pushstring(L, name);
		lua_pushvalue(L, -3);
		lua_rawset(L, -3);

		return 1;
	}

	return 0;
}

void GLuaGlobalAccessor::initialize()
{
	lua_State * L = this->scriptObject->getLuaState();

	if(! this->scriptObject->isGlobal()) {
		this->scriptObject->getTable();
	}

	lua_newtable(L);
	
	lua_pushstring(L, "__index");
	lua_pushlightuserdata(L, this);
	lua_pushcclosure(L, &GlobalAccessor_index, 1);
	lua_rawset(L, -3);

	lua_pushstring(L, "__newindex");
	lua_pushlightuserdata(L, this);
	lua_pushcclosure(L, &GlobalAccessor_newindex, 1);
	lua_rawset(L, -3);

	if(this->scriptObject->isGlobal()) {
#if HAS_LUA_GLOBALSINDEX
		lua_setmetatable(L, LUA_GLOBALSINDEX);
#else
		lua_pushglobaltable(L);
		lua_pushvalue(L, -2);
		lua_setmetatable(L, -2);
#endif
	}
	else {
		lua_setmetatable(L, -2);
	}
}


const char * luaOperators[] = {
	"__add", "__sub", "__mul", "__div",
	"__mod", "__unm", "__eq", "__lt",
	"__le", "__call"
};

const GMetaOpType metaOpTypes[] = {
	mopAdd, mopSub, mopMul, mopDiv,
	mopMod, mopNeg, mopEqual, mopLessor,
	mopLessorEqual, mopFunctor
};

const char * metaTableSignature = "cpgf_suPer_mEta_Table";
const lua_Integer metaTableSigValue = 0x1eabeef;
const char * classMetaTablePrefix = "cpgf_cLaSs_mEta_Table";


void error(lua_State * L, const char * message)
{
	lua_Debug ar;
	lua_getstack(L, 1, &ar);
	lua_getinfo(L, "nSl", &ar);

	const char * fileName = ar.source;
	char dummy = 0;
	if(fileName == NULL || *fileName != '@') {
		fileName = &dummy;
	}

	char s[1024];
	sprintf(s, "Error: file %.256s, line %d: %.700s", fileName, ar.currentline, message);

	lua_pushstring(L, s);
	lua_error(L);
}

void objectToLua(const GContextPointer & context, const GClassGlueDataPointer & classData, void * instance, bool allowGC, ObjectPointerCV cv, ObjectGlueDataType dataType)
{
	lua_State * L = getLuaState(context);

	if(instance == NULL) {
		lua_pushnil(L);

		return;
	}

	void * userData = lua_newuserdata(L, getGlueDataWrapperSize<GObjectGlueData>());
	GObjectGlueDataPointer objectData(context->newObjectGlueData(classData, instance, allowGC, cv, dataType));
	GGlueDataWrapper * dataWrapper = newGlueDataWrapper(userData, objectData);

	IMetaClass * metaClass = classData->getMetaClass();

	const char * className = metaClass->getName();
	
	GASSERT_MSG(strlen(className) < 1000, "Meta class name is too long");

	char metaTableName[1100];

	strcpy(metaTableName, classMetaTablePrefix);
	strcat(metaTableName, className);

	lua_getfield(L, LUA_REGISTRYINDEX, metaTableName);
	if(lua_isnil(L, -1)) {
		lua_pop(L, 1);

		lua_newtable(L);
		setMetaTableSignature(L);

		setMetaTableGC(L);
	
		initObjectMetaTable(L);

		lua_pushvalue(L, -1); // duplicate the meta table
		lua_setfield(L, LUA_REGISTRYINDEX, metaTableName);
	}
	doBindAllOperators(context, instance, metaClass);
	
	lua_setmetatable(L, -2);
}

void * luaToObject(const GContextPointer & context, int index, GMetaType * outType)
{
	lua_State * L = getLuaState(context);

	if(isValidMetaTable(L, index)) {
		void * userData = lua_touserdata(L, index);
		if(static_cast<GGlueDataWrapper *>(userData)->getData()->getType() == gdtObject) {
			GObjectGlueDataPointer objectData = static_cast<GGlueDataWrapper *>(userData)->getAs<GObjectGlueData>();
			if(outType != NULL) {
				GMetaTypeData typeData;
				objectData->getClassData()->getMetaClass()->getMetaType(&typeData);
				metaCheckError(objectData->getClassData()->getMetaClass());
				*outType = GMetaType(typeData);
			}

			return objectData->getInstance();
		}
	}

	return NULL;
}

GMetaVariant luaUserDataToVariant(const GContextPointer & context, int index)
{
	lua_State * L = getLuaState(context);

	if(isValidMetaTable(L, index)) {
		void * userData = lua_touserdata(L, index);
		return glueDataToVariant(static_cast<GGlueDataWrapper *>(userData)->getData());
	}

	return GMetaVariant();
}

GMetaVariant functionToVariant(const GContextPointer & context, int index)
{
	GScopedInterface<IScriptFunction> func(new ImplScriptFunction(new GLuaScriptFunction(context, index), true));
	
	return GMetaVariant(func.get(), GMetaType());
}

GMetaVariant tableToVariant(const GContextPointer & context, int index)
{
	lua_State * L = getLuaState(context);

	GScopedInterface<IScriptObject> scriptObject(new ImplScriptObject(new GLuaScriptObject(context->getService(), L, context->getConfig(), index), true));
	
	return GMetaVariant(scriptObject.get(), GMetaType());
}

GMetaVariant luaToVariant(const GContextPointer & context, int index)
{
	lua_State * L = getLuaState(context);

	int type = lua_type(L, index);

	switch(type) {
		case LUA_TNIL:
			return (void *)0;

		case LUA_TNUMBER:
			return lua_tonumber(L, index);

		case LUA_TBOOLEAN:
			return lua_toboolean(L, index);

		case LUA_TSTRING:
			return GMetaVariant(createStringVariant(lua_tostring(L, index)), createMetaType<char *>());

		case LUA_TUSERDATA:
			return luaUserDataToVariant(context, index);

		case LUA_TFUNCTION:
			return functionToVariant(context, index);

		case LUA_TLIGHTUSERDATA:
			break;

		case LUA_TTABLE:
			return tableToVariant(context, index);
			
		case LUA_TTHREAD:
			break;
	}

	return GMetaVariant();
}

bool rawToLua(const GContextPointer & context, const GVariant & value)
{
	if(context->getConfig().allowAccessRawData()) {
		lua_State * L = getLuaState(context);

		void * userData = lua_newuserdata(L, getGlueDataWrapperSize<GRawGlueData>());
		GRawGlueDataPointer rawData(context->newRawGlueData(value));
		GGlueDataWrapper * dataWrapper = newGlueDataWrapper(userData, rawData);

		lua_newtable(L);

		setMetaTableSignature(L);
		setMetaTableGC(L);

		lua_setmetatable(L, -2);

		return true;
	}

	return false;
}

struct GLuaMethods
{
	typedef bool ResultType;

	static ResultType doObjectToScript(const GContextPointer & context, const GClassGlueDataPointer & classData, void * instance, bool allowGC, ObjectPointerCV cv, ObjectGlueDataType dataType)
	{
		objectToLua(context, classData, instance, allowGC, cv, dataType);
		return true;
	}

	static ResultType doVariantToScript(const GContextPointer & context, const GVariant & value, const GMetaType & type, bool allowGC, bool allowRaw)
	{
		return variantToLua(context, value, type, allowGC, allowRaw);
	}
	
	static ResultType doRawToScript(const GContextPointer & context, const GVariant & value)
	{
		return rawToLua(context, value);
	}

	static ResultType doConverterToScript(const GContextPointer & context, const GVariant & value, IMetaConverter * converter)
	{
		return converterToScript<GLuaMethods>(context, value, converter);
	}

	static ResultType doClassToScript(const GContextPointer & context, IMetaClass * metaClass)
	{
		doBindClass(context, metaClass);
		return true;
	}

	static ResultType doStringToScript(const GContextPointer & context, const char * s)
	{
		lua_pushstring(getLuaState(context), s);

		return true;
	}

	static ResultType doWideStringToScript(const GContextPointer & context, const wchar_t * ws)
	{
		GScopedArray<char> s(wideStringToString(ws));
		lua_pushstring(getLuaState(context), s.get());

		return true;
	}

	static bool isSuccessResult(const ResultType & result)
	{
		return result;
	}
};

bool variantToLua(const GContextPointer & context, const GVariant & value, const GMetaType & type, bool allowGC, bool allowRaw)
{
	lua_State * L = getLuaState(context);

	GVariantType vt = static_cast<GVariantType>(value.getType() & ~byReference);
	
	if(vtIsEmpty(vt)) {
		lua_pushnil(L);

		return true;
	}

	if(vtIsBoolean(vt)) {
		lua_pushboolean(L, fromVariant<bool>(value));

		return true;
	}

	if(vtIsInteger(vt)) {
		lua_pushinteger(L, fromVariant<lua_Integer>(value));

		return true;
	}

	if(vtIsReal(vt)) {
		lua_pushnumber(L, fromVariant<lua_Number>(value));

		return true;
	}

	if(!vtIsInterface(vt) && canFromVariant<void *>(value) && objectAddressFromVariant(value) == NULL) {
		lua_pushnil(L);

		return true;
	}

	if(variantIsString(value)) {
		lua_pushstring(L, fromVariant<char *>(value));

		return true;
	}

	if(variantIsWideString(value)) {
		const wchar_t * ws = fromVariant<wchar_t *>(value);
		GScopedArray<char> s(wideStringToString(ws));
		lua_pushstring(L, s.get());
		return true;
	}

	return variantToScript<GLuaMethods >(context, value, type, allowGC, allowRaw);
}

GScriptDataType getLuaType(lua_State * L, int index, IMetaTypedItem ** typeItem)
{
	if(typeItem != NULL) {
		*typeItem = NULL;
	}
	
	switch(lua_type(L, index)) {
		case LUA_TNIL:
			return sdtNull;

		case LUA_TNUMBER:
		case LUA_TBOOLEAN:
			return sdtFundamental;

		case LUA_TSTRING:
			return sdtString;

		case LUA_TUSERDATA:
			if(isValidMetaTable(L, index)) {
				void * rawUserData = lua_touserdata(L, index);
				GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(rawUserData);
				switch(dataWrapper->getData()->getType()) {
				case gdtClass:
					if(typeItem != NULL) {
						*typeItem = dataWrapper->getAs<GClassGlueData>()->getMetaClass();
						(*typeItem)->addReference();
					}
					return sdtClass;

				case gdtObject:
					if(typeItem != NULL) {
						*typeItem = dataWrapper->getAs<GObjectGlueData>()->getClassData()->getMetaClass();
						(*typeItem)->addReference();
					}

					return sdtObject;

				case gdtMethod:
					return methodTypeToGlueDataType(dataWrapper->getAs<GMethodGlueData>()->getMethodType());

				case gdtEnum:
					if(typeItem != NULL) {
						*typeItem = dataWrapper->getAs<GEnumGlueData>()->getMetaEnum();
						(*typeItem)->addReference();
					}
					return sdtEnum;

				case gdtRaw:
					return sdtRaw;

				default:
					break;
				}
			}
			break;

		case LUA_TTABLE:
			return sdtScriptObject;

		case LUA_TFUNCTION: {
			lua_getupvalue(L, index, 1);
			if(lua_isnil(L, -1)) {
				lua_pop(L, 1);
			}
			else {
				void * rawUserData = lua_touserdata(L, -1);
				GGlueDataWrapper * dataWrapper = static_cast<GGlueDataWrapper *>(rawUserData);

				if(dataWrapper != NULL) {
					switch(dataWrapper->getData()->getType()) {
					case gdtMethod:
						return methodTypeToGlueDataType(dataWrapper->getAs<GMethodGlueData>()->getMethodType());

					default:
						break;
					}
				}
			}
			return sdtScriptMethod;
		}

	}
	
	return sdtUnknown;
}

void loadMethodParameters(const GContextPointer & context, GVariant * outputParams, int startIndex, size_t paramCount)
{
	for(size_t i = 0; i < paramCount; ++i) {
		outputParams[i] = luaToVariant(context, static_cast<int>(i) + startIndex).getValue();
	}
}

void loadMethodParamTypes(const GContextPointer & context, CallableParamDataType * outputTypes, int startIndex, size_t paramCount)
{
	lua_State * L = getLuaState(context);

	for(size_t i = 0; i < paramCount; ++i) {
		IMetaTypedItem * typeItem;
		outputTypes[i].dataType = getLuaType(L, static_cast<int>(i) + startIndex, &typeItem);
		outputTypes[i].typeItem.reset(typeItem);
	}
}

void loadCallableParam(const GContextPointer & context, InvokeCallableParam * callableParam, int startIndex)
{
	loadMethodParameters(context, callableParam->paramsData, startIndex, callableParam->paramCount);
	loadMethodParamTypes(context, callableParam->paramsType, startIndex, callableParam->paramCount);
}

bool methodResultToLua(const GContextPointer & context, IMetaCallable * callable, InvokeCallableResult * result)
{
	return methodResultToScript<GLuaMethods>(context, callable, result);
}

#if 0
int callbackInvokeMethodList(lua_State * L)
{
	ENTER_LUA()

	GObjectMethodUserData * userData = static_cast<GObjectMethodUserData *>(lua_touserdata(L, lua_upvalueindex(1)));

	InvokeCallableParam callableParam(lua_gettop(L));
	loadCallableParam(userData->getContext(), &callableParam, 1);
	
	GObjectUserData data(userData->getContext(), userData->getObjectData());
	InvokeCallableResult result = doInvokeMethodList(userData->getContext(), &data, userData->getMethodData(), &callableParam);
	
	methodResultToLua(userData->getContext(), result.callable.get(), &result);
	return result.resultCount;

	LEAVE_LUA(L, return 0)
}

int invokeConstructor(GObjectUserData * classUserData)
{
	lua_State * L = getLuaState(classUserData->getContext());

	int paramCount = lua_gettop(L) - 1;
	
	InvokeCallableParam callableParam(paramCount);
	loadCallableParam(classUserData->getContext(), &callableParam, 2);
	
	void * instance = doInvokeConstructor(classUserData->getContext()->getService(), classUserData->getObjectData()->getMetaClass(), &callableParam);

	if(instance != NULL) {
		GObjectUserData * instanceUserData = objectToLua(classUserData->getContext(), instance, classUserData->getObjectData()->getMetaClass(), true, opcvNone, cudtObject);
		instanceCreated(instanceUserData, classUserData);
	}
	else {
		raiseCoreException(Error_ScriptBinding_FailConstructObject);
	}

	return 1;
}

int invokeOperator(const GContextPointer & context, void * instance, IMetaClass * metaClass, GMetaOpType op)
{
	lua_State * L = getLuaState(context);

	int paramCount = lua_gettop(L);
	int startIndex = 1;

	if(op == mopFunctor) { // skip the first "func" parameter
		++startIndex;
		--paramCount;
	}
	
	if(op == mopNeg) {
		paramCount = 1; // Lua pass two parameters to __unm...
	}

	InvokeCallableParam callableParam(paramCount);
	loadCallableParam(param, &callableParam, startIndex);
	
	InvokeCallableResult result = doInvokeOperator(param, instance, metaClass, op, &callableParam);
	methodResultToLua(param, result.callable.get(), &result);
	return result.resultCount;
}

int UserData_gc(lua_State * L)
{
	ENTER_LUA()

	GScriptUserData * userData = static_cast<GScriptUserData *>(lua_touserdata(L, -1));
	userData->~GScriptUserData();
	
	return 0;
	
	LEAVE_LUA(L, return 0)
}

int UserData_call(lua_State * L)
{
	ENTER_LUA()

	GObjectUserData * userData = static_cast<GObjectUserData *>(lua_touserdata(L, lua_upvalueindex(1)));


	if(userData->getObjectData()->getInstance() == NULL) { // constructor
		return invokeConstructor(userData);
	}
	else {
		raiseCoreException(Error_ScriptBinding_InternalError_WrongFunctor);

		return 0;
	}
	
	LEAVE_LUA(L, return 0)
}

bool doIndexMemberData(const GContextPointer & context, IMetaAccessible * data, void * instance, bool instanceIsConst)
{
	bool r;
	if(accessibleToScript<GLuaMethods>(&r, param, data, instance, instanceIsConst)) {
		return r;
	}
	return false;
}

bool indexMemberData(GObjectUserData * userData, IMetaAccessible * data, void * instance)
{
	return doIndexMemberData(userData->getContext(), data, instance, userData->getObjectData()->getCV() == opcvConst);
}

bool indexMemberEnumType(GObjectUserData * userData, GMetaMapItem * mapItem)
{
	GScopedInterface<IMetaEnum> metaEnum(gdynamic_cast<IMetaEnum *>(mapItem->getItem()));

	doBindEnum(userData->getContext(), metaEnum.get());

	return true;
}

bool indexMemberEnumValue(GObjectUserData * userData, GMetaMapItem * mapItem)
{
	GScopedInterface<IMetaEnum> metaEnum(gdynamic_cast<IMetaEnum *>(mapItem->getItem()));

	GVariantData data;
	metaEnum->getValue(&data, static_cast<uint32_t>(mapItem->getEnumIndex()));
	metaCheckError(metaEnum);
	variantToLua(userData->getContext(), GVariant(data), GMetaType(), false, true);

	return true;
}

bool indexMemberClass(GObjectUserData * userData, GMetaMapItem * mapItem)
{
	GScopedInterface<IMetaClass> metaClass(gdynamic_cast<IMetaClass *>(mapItem->getItem()));

	doBindClass(userData->getContext(), metaClass.get());

	return true;
}

int UserData_index(lua_State * L)
{
	ENTER_LUA()

	GObjectUserData * userData = static_cast<GObjectUserData *>(lua_touserdata(L, -2));
	
	const char * name = lua_tostring(L, -1);
	
	GMetaClassTraveller traveller(userData->getObjectData()->getMetaClass(), userData->getObjectData()->getInstance());
	
	void * instance = NULL;
	IMetaClass * outDerived;

	for(;;) {
		GScopedInterface<IMetaClass> metaClass(traveller.next(&instance, &outDerived));
		GScopedInterface<IMetaClass> derived(outDerived);

		if(!metaClass) {
			lua_pushnil(L);
			return false;
		}
		
		GMetaMapItem * mapItem = findMetaMapItem(userData->getContext()->getMetaMap(), metaClass.get(), name);
		if(mapItem == NULL) {
			continue;
		}
		
		switch(mapItem->getType()) {
			case mmitField:
			case mmitProperty: {
				GScopedInterface<IMetaAccessible> data(gdynamic_cast<IMetaAccessible *>(mapItem->getItem()));
				if(allowAccessData(userData->getContext()->getConfig(), getObjectData(userData), data.get())) {
					if(indexMemberData(userData, data.get(), instance)) {
						return true;
					}
					else {
						lua_pushnil(L);
						return false;
					}
				}
			}
			   break;

			case mmitMethod:
			case mmitMethodList: {
				GMapItemMethodData * data = gdynamic_cast<GMapItemMethodData *>(mapItem->getData());
				if(data == NULL) {
					GScopedInterface<IMetaList> methodList(createMetaList());
					loadMethodList(&traveller, methodList.get(), userData->getContext()->getMetaMap(), mapItem, instance, name);

					GScopedInterface<IMetaClass> boundClass(selectBoundClass(metaClass.get(), derived.get()));

					data = new GMapItemMethodData(GMethodData(boundClass.get(), methodList.get(), name, udmtInternal));

					mapItem->setData(data);
				}
				doBindMethodList(userData->getContext(), getSharedObjectData(userData), data->getMethodData());
				return true;
			}

			case mmitEnum:
				if(! userData->getObjectData()->isInstance() || userData->getContext()->getConfig().allowAccessEnumTypeViaInstance()) {
					if(indexMemberEnumType(userData, mapItem)) {
						return true;
					}
				}
				break;

			case mmitEnumValue:
				if(! userData->getObjectData()->isInstance() || userData->getContext()->getConfig().allowAccessEnumValueViaInstance()) {
					if(indexMemberEnumValue(userData, mapItem)) {
						return true;
					}
				}
				break;

			case mmitClass:
				if(! userData->getObjectData()->isInstance() || userData->getContext()->getConfig().allowAccessClassViaInstance()) {
					if(indexMemberClass(userData, mapItem)) {
						return true;
					}
				}
				break;

			default:
				break;
		}
	}

	LEAVE_LUA(L, lua_pushnil(L); return false)
}

int UserData_newindex(lua_State * L)
{
	ENTER_LUA()

	GObjectUserData * userData = static_cast<GObjectUserData *>(lua_touserdata(L, -3));
	
	const char * name = lua_tostring(L, -2);

	if(doSetFieldValue(userData, name, luaToVariant(userData->getContext(), -1).getValue())) {
		return 1;
	}

	return 0;
	
	LEAVE_LUA(L, return 0)
}

int UserData_operator(lua_State * L)
{
	ENTER_LUA()
	
	GOperatorUserData * userData = static_cast<GOperatorUserData *>(lua_touserdata(L, lua_upvalueindex(1)));

	return invokeOperator(userData->getContext(), userData->getInstance(), userData->getMetaClass(), userData->getOp());
	
	LEAVE_LUA(L, return 0)
}

void doBindOperator(const GContextPointer & context, void * instance, IMetaClass * metaClass, GMetaOpType op)
{
	lua_State * L = getLuaState(context);

	for(size_t i = 0; i < sizeof(metaOpTypes) / sizeof(metaOpTypes[0]); ++i) {
		if(metaOpTypes[i] == op) {
			lua_pushstring(L, luaOperators[i]);
			void * userData = lua_newuserdata(L, sizeof(GOperatorUserData));
			new (userData) GOperatorUserData(param, instance, metaClass, op);

			lua_newtable(L);
			setMetaTableSignature(L);
			setMetaTableGC(L);
			lua_setmetatable(L, -2);

			lua_pushcclosure(L, &UserData_operator, 1);
			lua_rawset(L, -3);

			return;
		}
	}
}

void doBindAllOperators(const GContextPointer & context, void * instance, IMetaClass * metaClass)
{
	std::vector<uint32_t> boundOperators;

	int count = metaClass->getOperatorCount();
	for(int i = 0; i < count; ++i) {
		GScopedInterface<IMetaOperator> item(metaClass->getOperatorAt(i));
		uint32_t op = item->getOperator();
		if(std::find(boundOperators.begin(), boundOperators.end(), op) == boundOperators.end()) {
			doBindOperator(param, instance, metaClass, static_cast<GMetaOpType>(op));
		}
	}
}

void doBindClass(const GContextPointer & context, IMetaClass * metaClass)
{
	lua_State * L = getLuaState(context);

	void * userData = lua_newuserdata(L, sizeof(GObjectUserData));
	new (userData) GObjectUserData(param, metaClass, NULL, false, opcvNone, cudtClass);

	lua_newtable(L);

	setMetaTableSignature(L);
	setMetaTableGC(L);
	setMetaTableCall(L, userData);

	initObjectMetaTable(L);

	lua_setmetatable(L, -2);
}

void doBindMethodList(const GContextPointer & context, const GSharedObjectData & objectData, const GMethodData & data)
{
	lua_State * L = getLuaState(context);

	void * userData = lua_newuserdata(L, sizeof(GObjectMethodUserData));
	new (userData) GObjectMethodUserData(param, objectData, data);
	
	lua_newtable(L);
	
	setMetaTableSignature(L);
	setMetaTableGC(L);
	lua_setmetatable(L, -2);
	
	lua_pushcclosure(L, &callbackInvokeMethodList, 1);
}

void doBindMethodList(const GContextPointer & context, const char * name, IMetaList * methodList, GUserDataMethodType methodType)
{
	GMethodData data = GMethodData(NULL, methodList, name, methodType);
	doBindMethodList(param, GSharedObjectData(), data);
}

void setMetaTableGC(lua_State * L)
{
	lua_pushstring(L, "__gc");	
	lua_pushcclosure(L, &UserData_gc, 0);
	lua_rawset(L, -3);
}

void setMetaTableCall(lua_State * L, void * userData)
{
	lua_pushstring(L, "__call");
	lua_pushlightuserdata(L, userData);
	lua_pushcclosure(L, &UserData_call, 1);
	lua_rawset(L, -3);
}

void setMetaTableSignature(lua_State * L)
{
	lua_pushstring(L, metaTableSignature);
	lua_pushinteger(L, metaTableSigValue);
	lua_rawset(L, -3);
}

bool isValidMetaTable(lua_State * L, int index)
{
	if(lua_getmetatable(L, index) == 0) {
		return false;
	}

	lua_pushstring(L, metaTableSignature);
	lua_gettable(L, -2);
	bool valid = (lua_tointeger(L, -1) == metaTableSigValue);
	lua_pop(L, 2);
	return valid;
}

void initObjectMetaTable(lua_State * L)
{
	lua_pushstring(L, "__index");
	lua_pushcclosure(L, &UserData_index, 0);
	lua_rawset(L, -3);

	lua_pushstring(L, "__newindex");
	lua_pushcclosure(L, &UserData_newindex, 0);
	lua_rawset(L, -3);
}


int Enum_index(lua_State * L)
{
	ENTER_LUA()

	GEnumUserData * userData = static_cast<GEnumUserData *>(lua_touserdata(L, -2));
	
	const char * name = lua_tostring(L, -1);

	int index = userData->getMetaEnum()->findKey(name);
	if(index < 0) {
		raiseCoreException(Error_ScriptBinding_CantFindEnumKey, name);
	}
	else {
		GVariantData data;
		userData->getMetaEnum()->getValue(&data, index);
		lua_pushinteger(L, fromVariant<lua_Integer>(GVariant(data)));
	}
	
	return true;
	
	LEAVE_LUA(L, return false)
}

int Enum_newindex(lua_State * L)
{
	ENTER_LUA()

	raiseCoreException(Error_ScriptBinding_CantAssignToEnumMethodClass);

	return 0;
	
	LEAVE_LUA(L, return 0)
}

void doBindEnum(const GContextPointer & context, IMetaEnum * metaEnum)
{
	lua_State * L = getLuaState(context);

	void * userData = lua_newuserdata(L, sizeof(GEnumUserData));
	new (userData) GEnumUserData(param, metaEnum);

	lua_newtable(L);

	setMetaTableSignature(L);
	setMetaTableGC(L);
	
	lua_pushstring(L, "__index");
	lua_pushcclosure(L, &Enum_index, 0);
	lua_rawset(L, -3);

	lua_pushstring(L, "__newindex");
	lua_pushcclosure(L, &Enum_newindex, 0);
	lua_rawset(L, -3);

	lua_setmetatable(L, -2);
}


GLuaScopeGuard::GLuaScopeGuard(GScriptObject * scope)
	: scope(gdynamic_cast<GLuaScriptObject *>(scope))
{
	this->top = lua_gettop(this->scope->luaState);
	if(! this->scope->isGlobal()) {
		this->scope->getTable();
	}
}
	
GLuaScopeGuard::~GLuaScopeGuard()
{
	if(this->top >= 0) {
		int currentTop = lua_gettop(this->scope->luaState);
		if(currentTop > this->top) {
			lua_pop(this->scope->luaState, currentTop - this->top);
		}
	}
}

void GLuaScopeGuard::keepStack()
{
	this->top = -1;
}

void GLuaScopeGuard::set(const char * name)
{
	if(scope->isGlobal()) {
		lua_setglobal(this->scope->luaState, name);
	}
	else {
		lua_setfield(this->scope->luaState, -2, name);
	}
}
	
void GLuaScopeGuard::get(const char * name)
{
	if(scope->isGlobal()) {
		lua_getglobal(this->scope->luaState, name);
	}
	else {
		lua_getfield(this->scope->luaState, -1, name);
	}
}

void GLuaScopeGuard::rawGet(const char * name)
{
#if HAS_LUA_GLOBALSINDEX
	lua_pushstring(this->scope->luaState, name);

	if(scope->isGlobal()) {
		lua_rawget(this->scope->luaState, LUA_GLOBALSINDEX);
	}
	else {
		lua_rawget(this->scope->luaState, -2);
	}
#else
	if(scope->isGlobal()) {
		lua_pushglobaltable(this->scope->luaState);
	}

	lua_pushstring(this->scope->luaState, name);
	lua_rawget(this->scope->luaState, -2);
	lua_remove(this->scope->luaState, -2); // remove the global table to balance the stace
#endif
}

// function is on stack top
GMetaVariant invokeLuaFunctionIndirectly(const GBindingParamPointer & bindingParam, GMetaVariant const * const * params, size_t paramCount, const char * name)
{
	GASSERT_MSG(paramCount <= REF_MAX_ARITY, "Too many parameters.");

	if(! bindingParam) {
		raiseCoreException(Error_ScriptBinding_NoContext);
	}

	lua_State * L = getLuaState(bindingParam);

	int top = lua_gettop(L) - 1;

	if(lua_isfunction(L, -1)) {
		for(size_t i = 0; i < paramCount; ++i) {
			if(!variantToLua(bindingParam, params[i]->getValue(), params[i]->getType(), false, true)) {
				if(i > 0) {
					lua_pop(L, static_cast<int>(i) - 1);
				}

				raiseCoreException(Error_ScriptBinding_ScriptMethodParamMismatch, i, name);
			}
		}

		int error = lua_pcall(L, static_cast<int>(paramCount), LUA_MULTRET, 0);
		if(error) {
			raiseCoreException(Error_ScriptBinding_ScriptFunctionReturnError, name, lua_tostring(L, -1));
		}
		else {
			int resultCount = lua_gettop(L) - top;
			if(resultCount > 1) {
				raiseCoreException(Error_ScriptBinding_CantReturnMultipleValue, name);
			}
			else {
				if(resultCount > 0) {
					return luaToVariant(bindingParam, -1);
				}
			}
		}
	}
	else {
		raiseCoreException(Error_ScriptBinding_CantCallNonfunction);
	}
	
	return GMetaVariant();
}


GLuaScriptFunction::GLuaScriptFunction(const GContextPointer & context, int objectIndex)
	: context(context), ref(refLua(getLuaState(bindingParam), objectIndex))
{
}

GLuaScriptFunction::~GLuaScriptFunction()
{
	unrefLua(getLuaState(this->context.get()), this->ref);
}
	
GMetaVariant GLuaScriptFunction::invoke(const GMetaVariant * params, size_t paramCount)
{
	GASSERT_MSG(paramCount <= REF_MAX_ARITY, "Too many parameters.");

	const cpgf::GMetaVariant * variantPointers[REF_MAX_ARITY];

	for(size_t i = 0; i < paramCount; ++i) {
		variantPointers[i] = &params[i];
	}

	return this->invokeIndirectly(variantPointers, paramCount);
}

GMetaVariant GLuaScriptFunction::invokeIndirectly(GMetaVariant const * const * params, size_t paramCount)
{
	lua_State * L = getLuaState(this->bindingParam.get());

	ENTER_LUA()

	getRefObject(L, this->ref);

	return invokeLuaFunctionIndirectly(this->context.get(), params, paramCount, "");
	
	LEAVE_LUA(L, return GMetaVariant())
}


GLuaScriptObject::GLuaScriptObject(IMetaService * service, lua_State * L, const GScriptConfig & config)
	: super(config), context(new GLuaBindingContext(service, super::getConfig(), L)), luaState(L), ref(LUA_NOREF)
{
}

GLuaScriptObject::GLuaScriptObject(IMetaService * service, lua_State * L, const GScriptConfig & config, int objectIndex)
	: super(config), context(new GLuaBindingContext(service, super::getConfig(), L)), luaState(L), ref(LUA_NOREF)
{
	this->ref = refLua(this->luaState, objectIndex);
}

GLuaScriptObject::GLuaScriptObject(const GLuaScriptObject & other)
	: super(other.context->getConfig()), context(other.context), binding(binding), luaState(other.luaState), ref(LUA_NOREF)
{
	this->ref = refLua(this->luaState, -1);
}

GLuaScriptObject::~GLuaScriptObject()
{
	if(this->ref != LUA_NOREF) {
		unrefLua(this->luaState, this->ref);
	}
}

bool GLuaScriptObject::isGlobal() const
{
	return this->ref == LUA_NOREF;
}

void GLuaScriptObject::getTable() const
{
	if(this->ref != LUA_NOREF) {
		getRefObject(this->luaState, this->ref);
	}
	else {
#if HAS_LUA_GLOBALSINDEX
		lua_pushvalue(this->luaState, LUA_GLOBALSINDEX);
#else
		lua_pushglobaltable(this->luaState);
#endif
	}
}

GMethodGlueDataPointer GLuaScriptObject::doGetMethodUserData()
{
	if(lua_type(this->luaState, -1) != LUA_TFUNCTION) {
		return NULL;
	}

	lua_getupvalue(this->luaState, -1, 1);
	if(lua_isnil(this->luaState, -1)) {
		lua_pop(this->luaState, 1);
	}
	else {
		void * rawUserData = lua_touserdata(this->luaState, -1);
		GScriptUserData * userData = static_cast<GScriptUserData *>(rawUserData);

		if(userData->getType() == udtMethod) {
			GObjectMethodUserData * methodListData = static_cast<GObjectMethodUserData *>(userData);
			return methodListData;
		}
	}

	return NULL;
}

GLuaGlobalAccessor * GLuaScriptObject::getGlobalAccessor()
{
	if(! this->globalAccessor) {
		this->globalAccessor.reset(new GLuaGlobalAccessor(this->binding));
	}

	return this->globalAccessor.get();
}


GScriptDataType GLuaScriptObject::getType(const char * name, IMetaTypedItem ** outMetaTypeItem)
{
	ENTER_LUA()
	
	GLuaScopeGuard scopeGuard(this);
	
	scopeGuard.get(name);

	return getLuaType(this->luaState, -1, outMetaTypeItem);
	
	LEAVE_LUA(this->luaState, return sdtUnknown)
}

void GLuaScriptObject::bindClass(const char * name, IMetaClass * metaClass)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	doBindClass(this->param, metaClass);
	
	scopeGuard.set(name);

	LEAVE_LUA(this->luaState)
}

void GLuaScriptObject::bindEnum(const char * name, IMetaEnum * metaEnum)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	doBindEnum(this->param, metaEnum);
	
	scopeGuard.set(name);

	LEAVE_LUA(this->luaState)
}

GScriptObject * GLuaScriptObject::createScriptObject(const char * name)
{
	ENTER_LUA()
	
	GLuaScopeGuard scopeGuard(this);

	scopeGuard.get(name);

	if(lua_isnil(this->luaState, -1)) {
		lua_pop(this->luaState, 1);
		lua_newtable(this->luaState);
		scopeGuard.set(name);
		scopeGuard.get(name);
	}
	else {
		if(isValidMetaTable(this->luaState, -1)) {
			lua_pop(this->luaState, 1);
			return NULL;
		}
	}

	GLuaScriptObject * binding = new GLuaScriptObject(*this);
	binding->owner = this;
	binding->name = name;
	
	return binding;

	LEAVE_LUA(this->luaState, return NULL)
}

GScriptObject * GLuaScriptObject::gainScriptObject(const char * name)
{
	ENTER_LUA()
	
	GLuaScopeGuard scopeGuard(this);

	scopeGuard.get(name);

	if(lua_isnil(this->luaState, -1)) {
		lua_pop(this->luaState, 1);
		
		return NULL;
	}

//	if(isValidMetaTable(this->luaState, -1)) {
//		lua_pop(this->luaState, 1);
		
//		return NULL;
//	}

	GLuaScriptObject * binding = new GLuaScriptObject(*this);
	binding->owner = this;
	binding->name = name;
	
	return binding;

	LEAVE_LUA(this->luaState, return NULL)
}

GScriptFunction * GLuaScriptObject::gainScriptFunction(const char * name)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	scopeGuard.get(name);

	return new GLuaScriptFunction(this->getContext(), -1);
	
	LEAVE_LUA(this->luaState, return NULL)
}

GMetaVariant GLuaScriptObject::invoke(const char * name, const GMetaVariant * params, size_t paramCount)
{
	GASSERT_MSG(paramCount <= REF_MAX_ARITY, "Too many parameters.");

	const cpgf::GMetaVariant * variantPointers[REF_MAX_ARITY];

	for(size_t i = 0; i < paramCount; ++i) {
		variantPointers[i] = &params[i];
	}

	return this->invokeIndirectly(name, variantPointers, paramCount);
}

GMetaVariant GLuaScriptObject::invokeIndirectly(const char * name, GMetaVariant const * const * params, size_t paramCount)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	scopeGuard.get(name);

	return invokeLuaFunctionIndirectly(this->getContext(), params, paramCount, name);
	
	LEAVE_LUA(this->luaState, return GMetaVariant())
}

void GLuaScriptObject::bindFundamental(const char * name, const GVariant & value)
{
	GASSERT_MSG(vtIsFundamental(vtGetType(value.data.typeData)), "Only fundamental value can be bound via bindFundamental");

	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	if(! variantToLua(this->param, value, GMetaType(), false, true)) {
		raiseCoreException(Error_ScriptBinding_CantBindFundamental);
	}
	
	scopeGuard.set(name);

	LEAVE_LUA(this->luaState)
}

void GLuaScriptObject::bindAccessible(const char * name, void * instance, IMetaAccessible * accessible)
{
	ENTER_LUA()

	this->getGlobalAccessor()->doBindAccessible(name, instance, accessible);

	LEAVE_LUA(this->luaState)
}

void GLuaScriptObject::bindString(const char * stringName, const char * s)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	lua_pushstring(this->luaState, s);

	scopeGuard.set(stringName);

	LEAVE_LUA(this->luaState)
}

void GLuaScriptObject::bindObject(const char * objectName, void * instance, IMetaClass * type, bool transferOwnership)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	objectToLua(this->param, instance, gdynamic_cast<IMetaClass *>(type), transferOwnership, opcvNone, cudtObject);

	scopeGuard.set(objectName);

	LEAVE_LUA(this->luaState)
}

void GLuaScriptObject::bindRaw(const char * name, const GVariant & value)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	if(! rawToLua(this->param, value)) {
		raiseCoreException(Error_ScriptBinding_CantBindRaw);
	}
	
	scopeGuard.set(name);

	LEAVE_LUA(this->luaState)
}

void GLuaScriptObject::bindMethod(const char * name, void * instance, IMetaMethod * method)
{
	ENTER_LUA()

	if(method->isStatic()) {
		instance = NULL;
	}

	GLuaScopeGuard scopeGuard(this);
	
	GScopedInterface<IMetaList> methodList(createMetaList());
	methodList->add(method, instance);

	doBindMethodList(this->param, name, methodList.get(), udmtMethod);
	
	scopeGuard.set(name);
	
	LEAVE_LUA(this->luaState)
}

void GLuaScriptObject::bindMethodList(const char * name, IMetaList * methodList)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	doBindMethodList(this->param, name, methodList, udmtMethodList);
	
	scopeGuard.set(name);
	
	LEAVE_LUA(this->luaState)
}

IMetaClass * GLuaScriptObject::getClass(const char * className)
{
	IMetaTypedItem * typedItem = NULL;

	GScriptDataType sdt = this->getType(className, &typedItem);
	GScopedInterface<IMetaTypedItem> item(typedItem);
	if(sdt == sdtClass) {
		return gdynamic_cast<IMetaClass *>(item.take());
	}

	return NULL;
}

IMetaEnum * GLuaScriptObject::getEnum(const char * enumName)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	scopeGuard.get(enumName);

	if(isValidMetaTable(this->luaState, -1)) {
		void * userData = lua_touserdata(this->luaState, -1);
		if(static_cast<GScriptUserData *>(userData)->getType() == udtEnum) {
			GEnumUserData * enumData = static_cast<GEnumUserData *>(userData);

			IMetaEnum * metaEnum = enumData->getMetaEnum();
			metaEnum->addReference();
			return metaEnum;
		}
	}

	return NULL;
	
	LEAVE_LUA(this->luaState, return NULL)
}

GVariant GLuaScriptObject::getFundamental(const char * name)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	scopeGuard.get(name);
	
	if(getLuaType(this->luaState, -1, NULL) == sdtFundamental) {
		return luaToVariant(this->param, -1).getValue();
	}
	else {
		lua_pop(this->luaState, 1);
		
		return GVariant();
	}

	LEAVE_LUA(this->luaState, return GVariant())
}

std::string GLuaScriptObject::getString(const char * stringName)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	scopeGuard.get(stringName);

	return lua_tostring(this->luaState, -1);
	
	LEAVE_LUA(this->luaState, return "")
}

void * GLuaScriptObject::getObject(const char * objectName)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	scopeGuard.get(objectName);

	return luaToObject(this->param, -1, NULL);
	
	LEAVE_LUA(this->luaState, return NULL)
}

GVariant GLuaScriptObject::getRaw(const char * name)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	scopeGuard.get(name);
	
	if(getLuaType(this->luaState, -1, NULL) == sdtRaw) {
		return luaToVariant(this->param, -1).getValue();
	}
	else {
		lua_pop(this->luaState, 1);
		
		return GVariant();
	}

	LEAVE_LUA(this->luaState, return GVariant())
}

IMetaMethod * GLuaScriptObject::getMethod(const char * methodName, void ** outInstance)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	scopeGuard.get(methodName);
	
	if(outInstance != NULL) {
		*outInstance = NULL;
	}

	GObjectMethodUserData * userData = this->doGetMethodUserData();
	if(userData != NULL) {
		if(outInstance != NULL) {
			*outInstance = userData->getMethodData().getMethodList()->getInstanceAt(0);
		}

		return gdynamic_cast<IMetaMethod *>(userData->getMethodData().getMethodList()->getAt(0));
	}
	else {
		return NULL;
	}

	
	LEAVE_LUA(this->luaState, return NULL)
}

IMetaList * GLuaScriptObject::getMethodList(const char * methodName)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	scopeGuard.get(methodName);

	GObjectMethodUserData * userData = this->doGetMethodUserData();
	if(userData != NULL) {
		userData->getMethodData().getMethodList()->addReference();

		return userData->getMethodData().getMethodList();
	}
	else {
		return NULL;
	}

	LEAVE_LUA(this->luaState, return NULL)
}

void GLuaScriptObject::assignValue(const char * fromName, const char * toName)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	scopeGuard.get(fromName);
	scopeGuard.set(toName);

	LEAVE_LUA(this->luaState)
}

bool GLuaScriptObject::valueIsNull(const char * name)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	scopeGuard.get(name);

	return lua_isnil(this->luaState, -1);

	LEAVE_LUA(this->luaState, return false)
}

void GLuaScriptObject::nullifyValue(const char * name)
{
	ENTER_LUA()

	GLuaScopeGuard scopeGuard(this);

	scopeGuard.get(name);
	if(! lua_isnil(this->luaState, -1)) {
		lua_pop(this->luaState, 1);
		lua_pushnil(this->luaState);
		scopeGuard.set(name);
	}

	LEAVE_LUA(this->luaState)
}

void GLuaScriptObject::bindCoreService(const char * name)
{
	ENTER_LUA()

	this->param->bindScriptCoreService(this, name);

	LEAVE_LUA(this->luaState)
}

#endif

} // unnamed namespace


GScriptObject * new_createLuaScriptObject(IMetaService * service, lua_State * L, const GScriptConfig & config)
{
	return NULL; //new GLuaScriptObject(service, L, config);
}



} // namespace cpgf



#if defined(_MSC_VER)
#pragma warning(pop)
#endif
