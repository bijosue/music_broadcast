#ifndef __OPENSLES_COMMON_H__
#define __OPENSLES_COMMON_H__

#include <SLES/OpenSLES.h>
#include <stddef.h>
#include <memory>

const char* GetSLErrorString(size_t code);

SLDataFormat_PCM CreatePCMConfiguration(int sample_rate, size_t channels,size_t bits_per_sample);

template <typename SLType, typename SLDerefType>
class ScopedSLObject {
public:
    ScopedSLObject() : obj_(NULL) {}
    ~ScopedSLObject() { Reset(); }

    SLType* Receive() {
        return &obj_;
    }

    SLDerefType operator->() { return *obj_; }

    SLType Get() const { return obj_; }

    void Reset() {
        if (obj_) {
            (*obj_)->Destroy(obj_);
            obj_ = NULL;
        }
    }

private:
    SLType obj_;
};

typedef ScopedSLObject<SLObjectItf, const SLObjectItf_*> ScopedSLObjectItf;


class OpenSLEngineManager{
public:
    OpenSLEngineManager();
    ~OpenSLEngineManager(){};
    SLObjectItf GetOpenSLEngine();
private:
    ScopedSLObjectItf engine_object_;
};

extern std::unique_ptr<OpenSLEngineManager> engine_manager;

#endif