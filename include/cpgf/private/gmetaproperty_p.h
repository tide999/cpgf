#ifndef __GMETAPROPERTY_P_H
#define __GMETAPROPERTY_P_H

#include "cpgf/gmetacommon.h"
#include "cpgf/gmetatype.h"
#include "cpgf/gmetapolicy.h"
#include "cpgf/gcallback.h"
#include "cpgf/gifelse.h"


#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4127) // conditional expression is constant
#endif


namespace cpgf {

namespace meta_internal {

template <typename Getter, typename Policy, typename Enabled = void>
class GMetaGetter
{
public:
	typedef int PropertyType;
	
	G_STATIC_CONSTANT(bool, HasGetter = false);
	G_STATIC_CONSTANT(bool, Readable = false);

public:
	GMetaGetter(const Getter & getter) {
		(void)getter;
	}

	GVariant get(void * instance) const {
		(void)instance;

		return GVariant();
	}

	void * getPropertyAddress(void * instance) const {
		(void)instance;

		return NULL;
	}
};


template <typename Getter, typename Policy>
class GMetaGetter <Getter, Policy, typename GEnableIfResult<
	GAndResult<
		GNotResult<IsFunction<Getter> >,
		GNotResult<MemberDataTrait<Getter> >,
		GNotResult<IsFundamental<Getter> >
	>
	>::Result
>
{
	GASSERT_STATIC(IsPointer<Getter>::Result);

public:
	G_STATIC_CONSTANT(bool, HasGetter = true);
	G_STATIC_CONSTANT(bool, Readable = (PolicyNotHasRule<Policy, GMetaRuleForbidRead>::Result));

public:
	typedef typename RemovePointer<Getter>::Result PropertyType;

public:
	GMetaGetter(const Getter & getter) : getter(getter) {
	}

	GMetaGetter(const GMetaGetter & other) : getter(other.getter) {
	}

	GMetaGetter & operator = (const GMetaGetter & other) {
		if(this != &other) {
			this->getter = other.getter;
		}
	}

	GVariant get(void * instance) const {
		return this->doGet<void>(instance);
	}
	
	void * getPropertyAddress(void * instance) const {
		(void)instance;

		return this->getter;
	}

private:	
	template <typename T>
	GVariant doGet(typename GEnableIf<Readable, T>::Result * instance) const {
		(void)instance;

		GVarTypeData data = GVarTypeData();
		deduceVariantType<PropertyType>(data, true);
		return GVariant(data, *(this->getter));
	}
	
	template <typename T>
	GVariant doGet(typename GDisableIf<Readable, T>::Result * instance) const {
		(void)instance;

		meta_internal::handleForbidAccessError(true);
		
		return GVariant();
	}
	

private:
	Getter getter;
};

template <typename Getter, typename Policy>
class GMetaGetter <Getter, Policy, typename GEnableIfResult<
	GAndResult<
		GNotResult<IsFunction<Getter> >,
		MemberDataTrait<Getter>
	>
	>::Result
>
{
public:
	G_STATIC_CONSTANT(bool, HasGetter = true);
	G_STATIC_CONSTANT(bool, Readable = (PolicyNotHasRule<Policy, GMetaRuleForbidRead>::Result));

public:
	typedef typename MemberDataTrait<Getter>::FieldType PropertyType;

public:
	GMetaGetter(const Getter & getter) : getter(getter) {
	}

	GMetaGetter(const GMetaGetter & other) : getter(other.getter) {
	}

	GMetaGetter & operator = (const GMetaGetter & other) {
		if(this != &other) {
			this->getter = other.getter;
		}
	}

	GVariant get(void * instance) const {
		return this->doGet<void>(instance);
	}
	
	void * getPropertyAddress(void * instance) const {
		return &(static_cast<typename MemberDataTrait<Getter>::ObjectType *>(instance)->*(this->getter));
	}

private:	
	template <typename T>
	GVariant doGet(typename GEnableIf<Readable, T>::Result * instance) const {
		GVarTypeData data = GVarTypeData();
		deduceVariantType<PropertyType>(data, true);
		return GVariant(data, static_cast<typename MemberDataTrait<Getter>::ObjectType *>(instance)->*(this->getter));
	}

	template <typename T>
	GVariant doGet(typename GDisableIf<Readable, T>::Result * instance) const {
		(void)instance;

		meta_internal::handleForbidAccessError(true);
		
		return GVariant();
	}
	
private:
	Getter getter;
};

template <typename Getter, typename Policy>
class GMetaGetter <Getter, Policy, typename GEnableIfResult<IsFunction<Getter> >::Result>
{
public:
	G_STATIC_CONSTANT(bool, HasGetter = true);
	G_STATIC_CONSTANT(bool, Readable = (PolicyNotHasRule<Policy, GMetaRuleForbidRead>::Result));
	G_STATIC_CONSTANT(bool, ExplicitThis = (PolicyHasRule<Policy, GMetaRuleExplicitThis>::Result || PolicyHasRule<Policy, GMetaRuleGetterExplicitThis>::Result));
	G_STATIC_CONSTANT(bool, NotExplicitThis = !ExplicitThis);

public:
	typedef typename GFunctionTraits<Getter>::ResultType PropertyType;

public:
	GMetaGetter(const Getter & getter) : callback(makeCallback(getter)) {
	}

	GMetaGetter(const GMetaGetter & other) : callback(other.callback) {
	}

	GMetaGetter & operator = (const GMetaGetter & other) {
		if(this != &other) {
			this->callback = other.callback;
		}
	}

	GVariant get(void * instance) const {
		return this->doGet<void>(instance);
	}
	
	void * getPropertyAddress(void * instance) const {
		(void)instance;

		return NULL;
	}

private:	
	template <typename T>
	GVariant doGet(typename GEnableIf<Readable && NotExplicitThis, T>::Result * instance) const {
		this->callback.setObject(instance);
		
		GVarTypeData data = GVarTypeData();
		deduceVariantType<PropertyType>(data, true);
		return GVariant(data, this->callback());
	}

	template <typename T>
	GVariant doGet(typename GEnableIf<Readable && ExplicitThis, T>::Result * instance) const {
		this->callback.setObject(instance);
		
		GVarTypeData data = GVarTypeData();
		deduceVariantType<PropertyType>(data, true);
		return GVariant(data, this->callback((typename GFunctionTraits<Getter>::ArgList::Arg0)(instance)));
	}

	template <typename T>
	GVariant doGet(typename GDisableIf<Readable, T>::Result * instance) const {
		(void)instance;

		meta_internal::handleForbidAccessError(true);
		
		return GVariant();
	}
	
private:
	typename FunctionCallbackType<Getter>::Result callback;
};


template <typename Setter, typename Policy, typename Enabled = void>
class GMetaSetter
{
public:
	typedef int PropertyType;

	G_STATIC_CONSTANT(bool, HasSetter = false);
	G_STATIC_CONSTANT(bool, Writable = false);

public:
	GMetaSetter(const Setter & setter) {
		(void)setter;
	}

	void set(void * instance, const GVariant & value) const {
		(void)instance;
		(void)value;
	}

};


template <typename Setter, typename Policy>
class GMetaSetter <Setter, Policy, typename GEnableIfResult<
	GAndResult<
		GNotResult<IsFunction<Setter> >,
		GNotResult<MemberDataTrait<Setter> >,
		GNotResult<IsFundamental<Setter> >
	>
	>::Result
>
{
	GASSERT_STATIC(IsPointer<Setter>::Result);

public:
	G_STATIC_CONSTANT(bool, HasSetter = true);
	G_STATIC_CONSTANT(bool, Writable = (PolicyNotHasRule<Policy, GMetaRuleForbidWrite>::Result));

public:
	typedef typename RemovePointer<Setter>::Result PropertyType;

public:
	GMetaSetter(const Setter & setter) : setter(setter) {
	}

	GMetaSetter(const GMetaSetter & other) : setter(other.setter) {
	}

	GMetaSetter & operator = (const GMetaSetter & other) {
		if(this != &other) {
			this->setter = other.setter;
		}
	}

	void set(void * instance, const GVariant & value) const {
		this->doSet<void>(instance, value);
	}

private:	
	template <typename T>
	void doSet(typename GEnableIf<Writable, T>::Result * instance, const GVariant & value) const {
		(void)instance;

		*(this->setter) = fromVariant<PropertyType>(value);
	}

	template <typename T>
	void doSet(typename GDisableIf<Writable, T>::Result * instance, const GVariant & value) const {
		(void)instance;
		(void)value;

		meta_internal::handleForbidAccessError(false);
	}

private:
	Setter setter;
};

template <typename Setter, typename Policy>
class GMetaSetter <Setter, Policy, typename GEnableIfResult<
	GAndResult<
		GNotResult<IsFunction<Setter> >,
		MemberDataTrait<Setter>
	>
	>::Result
>
{
public:
	G_STATIC_CONSTANT(bool, HasSetter = true);
	G_STATIC_CONSTANT(bool, Writable = (PolicyNotHasRule<Policy, GMetaRuleForbidWrite>::Result));

public:
	typedef typename MemberDataTrait<Setter>::FieldType PropertyType;

public:
	GMetaSetter(const Setter & setter) : setter(setter) {
	}

	GMetaSetter(const GMetaSetter & other) : setter(other.setter) {
	}

	GMetaSetter & operator = (const GMetaSetter & other) {
		if(this != &other) {
			this->setter = other.setter;
		}
	}

	void set(void * instance, const GVariant & value) const {
		this->doSet<void>(instance, value);
	}

private:	
	template <typename T>
	void doSet(typename GEnableIf<Writable, T>::Result * instance, const GVariant & value) const {
		static_cast<typename MemberDataTrait<Setter>::ObjectType *>(instance)->*(this->setter) = fromVariant<PropertyType>(value);
	}

	template <typename T>
	void doSet(typename GDisableIf<Writable, T>::Result * instance, const GVariant & value) const {
		(void)instance;
		(void)value;

		meta_internal::handleForbidAccessError(false);
	}

private:
	Setter setter;
};

template <typename Setter, typename Policy>
class GMetaSetter <Setter, Policy, typename GEnableIfResult<IsFunction<Setter> >::Result>
{
public:
	G_STATIC_CONSTANT(bool, HasSetter = true);
	G_STATIC_CONSTANT(bool, Writable = (PolicyNotHasRule<Policy, GMetaRuleForbidWrite>::Result));
	G_STATIC_CONSTANT(bool, ExplicitThis = (PolicyHasRule<Policy, GMetaRuleExplicitThis>::Result || PolicyHasRule<Policy, GMetaRuleSetterExplicitThis>::Result));
	G_STATIC_CONSTANT(bool, NotExplicitThis = !ExplicitThis);

public:
	typedef typename GFunctionTraits<Setter>::ArgList::Arg0 PropertyType;

public:
	GMetaSetter(const Setter & setter) : callback(makeCallback(setter)) {
	}

	GMetaSetter(const GMetaSetter & other) : callback(other.callback) {
	}

	GMetaSetter & operator = (const GMetaSetter & other) {
		if(this != &other) {
			this->callback = other.callback;
		}
	}

	void set(void * instance, const GVariant & value) const {
		this->doSet<void>(instance, value);
	}

private:	
	template <typename T>
	void doSet(typename GEnableIf<Writable && NotExplicitThis, T>::Result * instance, const GVariant & value) const {
		this->callback.setObject(instance);
		this->callback(fromVariant<PropertyType, typename GIfElseResult<PolicyHasRule<Policy, GMetaRuleCopyConstReference<0> >, VarantCastCopyConstRef, VarantCastKeepConstRef>::Result>(value));
	}

	template <typename T>
	void doSet(typename GEnableIf<Writable && ExplicitThis, T>::Result * instance, const GVariant & value) const {
		this->callback.setObject(instance);
		this->callback((typename GFunctionTraits<Setter>::ArgList::Arg0)(instance),
			fromVariant<typename GFunctionTraits<Setter>::ArgList::Arg1, typename GIfElseResult<PolicyHasRule<Policy, GMetaRuleCopyConstReference<0> >, VarantCastCopyConstRef, VarantCastKeepConstRef>::Result>(value));
	}

	template <typename T>
	void doSet(typename GDisableIf<Writable, T>::Result * instance, const GVariant & value) const {
		(void)instance;
		(void)value;

		meta_internal::handleForbidAccessError(false);
	}

private:
	typename FunctionCallbackType<Setter>::Result callback;
};


class GMetaPropertyDataBase
{
public:
	virtual ~GMetaPropertyDataBase() {}
	virtual bool canGet() const = 0;
	virtual bool canSet() const = 0;
	virtual GVariant get(void * instance) const = 0;
	virtual void set(void * instance, const GVariant & value) const = 0;
	virtual void * getPropertyAddress(void * instance) const = 0;
	virtual size_t getPropertySize() const = 0;
	virtual GMetaConverter * createConverter() const = 0;
};

template <typename Getter, typename Setter, typename Policy>
class GMetaPropertyData : public GMetaPropertyDataBase
{
private:
	typedef GMetaGetter<Getter, Policy> GetterType;
	typedef GMetaSetter<Setter, Policy> SetterType;
	
public:
	GMetaPropertyData(const Getter & getter, const Setter & setter) : metaGetter(getter), metaSetter(setter) {
	}

	virtual bool canGet() const {
		return GetterType::Readable;
	}

	virtual bool canSet() const {
		return SetterType::Writable;
	}

	virtual GVariant get(void * instance) const {
		return this->metaGetter.get(instance);
	}

	virtual void set(void * instance, const GVariant & value) const {
		this->metaSetter.set(instance, value);
	}

	virtual void * getPropertyAddress(void * instance) const {
		return this->metaGetter.getPropertyAddress(instance);
	}

	virtual size_t getPropertySize() const {
		if(GetterType::HasGetter) {
			return sizeof(typename GetterType::PropertyType);
		}

		if(SetterType::HasSetter) {
			return sizeof(typename SetterType::PropertyType);
		}

		meta_internal::handleForbidAccessError(true);
		
		return 0;
	}

	virtual GMetaConverter * createConverter() const {
		if(GetterType::HasGetter) {
			return GMetaConverterTraits<typename GetterType::PropertyType>::createConverter();
		}

		if(SetterType::HasSetter) {
			return GMetaConverterTraits<typename SetterType::PropertyType>::createConverter();
		}

		return NULL;
	}

private:
	GMetaGetter<Getter, Policy> metaGetter;
	GMetaSetter<Setter, Policy> metaSetter;
};


template <typename Getter, typename Setter, typename Policy>
GMetaType createPropertyType() {
	if(GMetaGetter<Getter, Policy>::HasGetter) {
		return createMetaType<typename GMetaGetter<Getter, Policy>::PropertyType>();
	}

	if(GMetaSetter<Setter, Policy>::HasSetter) {
		return createMetaType<typename GMetaSetter<Setter, Policy>::PropertyType>();
	}

	return GMetaType();
}

} // namespace meta_internal




} // namespace cpgf


#if defined(_MSC_VER)
#pragma warning(pop)
#endif



#endif
