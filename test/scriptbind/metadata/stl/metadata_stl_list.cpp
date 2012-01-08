#include "metadata_stl_common.h"
#include "cpgf/metadata/stl/gmetadata_list.h"
#include "cpgf/metadata/gmetadatapolicydeduce.h"

#include <list>


namespace {


template <typename Container, typename Binding>
void testStlList_Helper(Binding * binding, TestScriptContext * c, const char * className, const GTestValueProvider & valueProvider)
{
	GMetaDataNameReplacer replacer;
	initializeLuaReplacer(&replacer);
	GDefineMetaClass<Container> define = GDefineMetaClass<Container>::declare(className);
	buildMetaData_list(define, true, typename GMetaDataPolicyDeduce<typename Container::value_type>::Result(), &replacer);

	GScopedInterface<IMetaClass> metaClass(static_cast<IMetaClass *>(metaItemToInterface(define.getMetaClass())));
	binding->bindClass(className, metaClass.get());

	GScopedPointer<TestScriptContext> context(c);

	testStlContainer_Empty<Container>(binding, c, className, valueProvider);
	testStlContainer_Pushback<Container>(binding, c, className, valueProvider);
	testStlContainer_Popback<Container>(binding, c, className, valueProvider);
	testStlContainer_FrontBack<Container>(binding, c, className, valueProvider);
	testStlContainer_Assign<Container>(binding, c, className, valueProvider);

	testStlContainer_ForwardIterator<Container>(binding, c, className, valueProvider);
	testStlContainer_BackwardIterator<Container>(binding, c, className, valueProvider);
	testStlContainer_ReverseForwardIterator<Container>(binding, c, className, valueProvider);
	testStlContainer_ReverseBackwardIterator<Container>(binding, c, className, valueProvider);
	
	testStlContainer_Swap<Container>(binding, c, className, valueProvider);
	testStlContainer_Erase<Container>(binding, c, className, valueProvider);
	testStlContainer_Insert<Container>(binding, c, className, valueProvider);
	testStlContainer_Clear<Container>(binding, c, className, valueProvider);

	context.reset();
}

template <typename Binding>
void test_stl_list_int(Binding * binding, TestScriptContext * context)
{
	testStlList_Helper<list<int> >(binding, context, "vector_int", GTestValueProvider_Int(context));
}


#define CASE test_stl_list_int
#include "metadata_stl_testcase.h"


template <typename Binding>
void test_stl_list_object(Binding * binding, TestScriptContext * context)
{
	testStlList_Helper<list<StlTestData> >(binding, context, "vector_obj", GTestValueProvider_TestData(context));
}


#define CASE test_stl_list_object
#include "metadata_stl_testcase.h"


template <typename Binding>
void test_stl_list_string(Binding * binding, TestScriptContext * context)
{
	testStlList_Helper<list<string> >(binding, context, "vector_string", GTestValueProvider_String(context));
}


#define CASE test_stl_list_string
#include "metadata_stl_testcase.h"



}