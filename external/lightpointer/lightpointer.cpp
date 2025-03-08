#include <stdio.h>
#include <utils/RefBase.h>

using namespace android;

// LightClass 必须继承 LightRefBase<LightClass>
class LightClass : public LightRefBase<LightClass>
{
public:
    LightClass(){
        printf("LightClass 构造函数被调用.\n");
    }
    ~LightClass(){
        printf("LightClass 析构函数被调用.\n");
    }
};

int main(int argc, char const *argv[])
{
    LightClass *p = new LightClass();
    // 普通构造函数
    sp<LightClass> sp1(p);
    printf("sp1 引用计数: %d\n", sp1->getStrongCount());
    {   // 拷贝构造函数
        sp<LightClass> sp2(sp1);
        printf("sp1 引用计数: %d\n", sp1->getStrongCount());
        printf("sp2 引用计数: %d\n", sp2->getStrongCount());
    }
    printf("sp1 引用计数: %d\n", sp1->getStrongCount());
    return 0;
}
