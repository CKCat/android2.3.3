#include <stdio.h>
#include <utils/RefBase.h>

#define INITIAL_STRONG_VALUE (1<<28)

using namespace android;

#define INITIAL_STRONG_VALUE (1<<28)

class WeightClass : public RefBase
{
public:
	void printRefCount()
	{
		int32_t strong = getStrongCount();
		weakref_type* ref = getWeakRefs();

		printf("-----------------------\n");
		printf("强引用计数: %d.\n", (strong == INITIAL_STRONG_VALUE ? 0 : strong));
		printf("弱引用计数: %d.\n", ref->getWeakCount());
		printf("-----------------------\n");
	}
};

class StrongClass : public WeightClass
{
public:
	StrongClass()
	{
		printf("StrongClass 构造函数被调用.\n");
	}

	virtual ~StrongClass()
	{
		printf("StrongClass 析构函数被调用.\n");
	}
};

class WeakClass : public WeightClass
{
public:
	WeakClass()
	{
		extendObjectLifetime(OBJECT_LIFETIME_WEAK);
		printf("WeakClass 构造函数被调用.\n");
	}

	virtual ~WeakClass()
	{
		printf("WeakClass 析构函数被调用.\n");
	}
};

class ForeverClass : public WeightClass
{
public:
	ForeverClass()
	{
		extendObjectLifetime(OBJECT_LIFETIME_FOREVER);
		printf("ForeverClass 构造函数被调用.\n");
	}

	virtual ~ForeverClass()
	{
		printf("ForeverClass 析构函数被调用.\n");
	}
};


void TestStrongClass(StrongClass* pStrongClass)
{
	wp<StrongClass> wpOut = pStrongClass;
	pStrongClass->printRefCount();

	{
		sp<StrongClass> spInner = pStrongClass;
		pStrongClass->printRefCount();
	}

	sp<StrongClass> spOut = wpOut.promote();
	printf("TestStrongClass spOut: %p.\n", spOut.get());
}

void TestWeakClass(WeakClass* pWeakClass)
{
	wp<WeakClass> wpOut = pWeakClass;
	pWeakClass->printRefCount();

	{
		sp<WeakClass> spInner = pWeakClass;
		pWeakClass->printRefCount();
	}

	pWeakClass->printRefCount();
	sp<WeakClass> spOut = wpOut.promote();
	printf("TestWeakClass spOut: %p.\n", spOut.get());
}

void TestForeverClass(ForeverClass* pForeverClass)
{
	wp<ForeverClass> wpOut = pForeverClass;
	pForeverClass->printRefCount();

	{
		sp<ForeverClass> spInner = pForeverClass;
		pForeverClass->printRefCount();
	}
}

int main(int argc, char* argv[]) {

	printf("测试强指针引用: \n");
	StrongClass* pStrongClass = new StrongClass();
	TestStrongClass(pStrongClass);

	printf("\n测试弱指针引用: \n");
	WeakClass* pWeakClass = new WeakClass();
	TestWeakClass(pWeakClass);

	printf("\n测试 Forever 指针: \n");
	ForeverClass* pForeverClass = new ForeverClass();
	TestForeverClass(pForeverClass);
	pForeverClass->printRefCount();
	delete pForeverClass;


	return 0;
}
