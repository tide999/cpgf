#ifndef __GSCRIPTBINDUTIL_H
#define __GSCRIPTBINDUTIL_H


#include "cpgf/scriptbind/gscriptbind.h"
#include "cpgf/scriptbind/gscriptbindapi.h"

namespace cpgf {


#define DEF_LOAD_PARAM_HELPER(N, unused) params[N] = &GPP_CONCAT(p, N);
#define DEF_LOAD_PARAM(N) \
	const GVariant * params[N == 0 ? 1 : N]; \
	GPP_REPEAT_3(N, DEF_LOAD_PARAM_HELPER, GPP_EMPTY())

#define DEF_LOAD_PARAM_HELPER_API(N, unused) params[N] = GPP_CONCAT(p, N).getValue().refData();
#define DEF_LOAD_PARAM_API(N) \
	GVariantData params[N == 0 ? 1 : N]; \
	GPP_REPEAT_3(N, DEF_LOAD_PARAM_HELPER_API, GPP_EMPTY())

#define DEF_CALL_HELPER(N, unused) \
	inline GVariant invokeScriptFunction(GScriptObject * scriptObject, const char * name GPP_COMMA_IF(N) GPP_REPEAT_PARAMS(N, const GTypedVariant & p)) { \
		DEF_LOAD_PARAM(N) \
		return scriptObject->invokeIndirectly(name, params, N); \
	} \
	inline GVariant invokeScriptFunction(IScriptObject * scriptObject, const char * name GPP_COMMA_IF(N) GPP_REPEAT_PARAMS(N, const GTypedVariant & p)) { \
		DEF_LOAD_PARAM_API(N) \
		GVariant result; \
		scriptObject->invoke(&result.refData(), name, params, N); \
		return result; \
	} \
	inline GVariant invokeScriptFunction(GScriptFunction * scriptFunction GPP_COMMA_IF(N) GPP_REPEAT_PARAMS(N, const GTypedVariant & p)) { \
		DEF_LOAD_PARAM(N) \
		return scriptFunction->invokeIndirectly(params, N); \
	} \
	inline GVariant invokeScriptFunction(IScriptFunction * scriptFunction GPP_COMMA_IF(N) GPP_REPEAT_PARAMS(N, const GTypedVariant & p)) { \
		DEF_LOAD_PARAM_API(N) \
		GVariant result; \
		scriptFunction->invoke(&result.refData(), params, N); \
		return result; \
	}

GPP_REPEAT_2(REF_MAX_ARITY, DEF_CALL_HELPER, GPP_EMPTY())

#undef DEF_CALL_HELPER
#undef DEF_LOAD_PARAM
#undef DEF_LOAD_PARAM_HELPER
#undef DEF_LOAD_PARAM_API
#undef DEF_LOAD_PARAM_HELPER_API

GVariant scriptGetFundamental(GScriptObject * scriptObject, const char * name);
GVariant scriptGetFundamental(IScriptObject * scriptObject, const char * name);

IScriptObject * scriptObjectToInterface(GScriptObject * scriptObject, bool freeObject);
IScriptObject * scriptObjectToInterface(GScriptObject * scriptObject);

void injectObjectToScript(IScriptObject * scriptObject, IMetaClass * metaClass, void * instance);


} // namespace cpgf




#endif

