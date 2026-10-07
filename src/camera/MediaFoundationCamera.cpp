// Windows 头必须先于 Qt 头包含
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include "camera/MediaFoundationCamera.h"

namespace vi {

namespace {
// MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING 属性需要
// 让 SourceReader 能把设备原生格式转换为请求的 RGB32
} // namespace

MediaFoundationCamera::MediaFoundationCamera() = default;

MediaFoundationCamera::~MediaFoundationCamera() {
    close();
}

bool MediaFoundationCamera::open() {
    QMutexLocker locker(&mutex_);
    if (reader_)
        return true;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // 已初始化(S_FALSE)或模式冲突(RPC_E_CHANGED_MODE)也继续
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        return false;

    hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr))
        return false;
    mfInit_ = true;

    return startSource();
}

bool MediaFoundationCamera::startSource() {
    // 1. 枚举系统视频采集设备
    IMFAttributes* attrs = nullptr;
    if (FAILED(MFCreateAttributes(&attrs, 1)))
        return false;
    attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                   MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    IMFActivate** devices = nullptr;
    UINT32 count = 0;
    const HRESULT hrEnum = MFEnumDeviceSources(attrs, &devices, &count);
    attrs->Release();
    if (FAILED(hrEnum) || count == 0)
        return false; // 没有摄像头

    // 2. 激活第一个设备为 Media Source
    HRESULT hr = devices[0]->ActivateObject(IID_PPV_ARGS(&source_));
    for (UINT32 i = 0; i < count; ++i)
        devices[i]->Release();
    CoTaskMemFree(devices);
    if (FAILED(hr))
        return false;

    // 3. 创建 SourceReader，并启用视频处理以便转换到 RGB32
    IMFAttributes* rdAttrs = nullptr;
    MFCreateAttributes(&rdAttrs, 1);
    rdAttrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    hr = MFCreateSourceReaderFromMediaSource(source_, rdAttrs, &reader_);
    rdAttrs->Release();
    if (FAILED(hr)) {
        source_->Release();
        source_ = nullptr;
        return false;
    }

    // 4. 请求 RGB32 输出子类型
    IMFMediaType* mt = nullptr;
    MFCreateMediaType(&mt);
    mt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    mt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    hr = reader_->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, mt);
    mt->Release();
    if (FAILED(hr))
        return false;

    // 5. 读取图像尺寸与行步长
    IMFMediaType* cur = nullptr;
    if (SUCCEEDED(reader_->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &cur))) {
        UINT32 w = 0, h = 0;
        if (SUCCEEDED(MFGetAttributeSize(cur, MF_MT_FRAME_SIZE, &w, &h))) {
            width_ = int(w);
            height_ = int(h);
        }
        UINT32 stride = 0;
        if (SUCCEEDED(cur->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride)))
            stride_ = long(stride);
        cur->Release();
    }
    if (stride_ == 0)
        stride_ = long(width_) * 4; // RGB32 = 每像素 4 字节

    return width_ > 0 && height_ > 0;
}

void MediaFoundationCamera::close() {
    QMutexLocker locker(&mutex_);
    if (reader_) {
        reader_->Release();
        reader_ = nullptr;
    }
    if (source_) {
        source_->Shutdown();
        source_->Release();
        source_ = nullptr;
    }
    if (mfInit_) {
        MFShutdown();
        mfInit_ = false;
    }
    width_ = 0;
    height_ = 0;
    stride_ = 0;
}

bool MediaFoundationCamera::isOpen() const {
    QMutexLocker locker(&mutex_);
    return reader_ != nullptr;
}

bool MediaFoundationCamera::grabFrame(QImage& frame, int timeoutMs) {
    Q_UNUSED(timeoutMs)
    QMutexLocker locker(&mutex_);
    if (!reader_ || width_ <= 0 || height_ <= 0)
        return false;

    // 采集线程首次调用时需初始化 COM（重复调用返回 S_FALSE，幂等）
    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hrCom) && hrCom != RPC_E_CHANGED_MODE)
        return false;

    for (int attempt = 0; attempt < 5; ++attempt) {
        DWORD streamIndex = 0, flags = 0;
        LONGLONG ts = 0;
        IMFSample* sample = nullptr;
        const HRESULT hr = reader_->ReadSample(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
            &streamIndex, &flags, &ts, &sample);

        if (SUCCEEDED(hr) && sample) {
            IMFMediaBuffer* buffer = nullptr;
            if (SUCCEEDED(sample->ConvertToContiguousBuffer(&buffer))) {
                BYTE* data = nullptr;
                DWORD maxLen = 0, curLen = 0;
                if (SUCCEEDED(buffer->Lock(&data, &maxLen, &curLen)) &&
                    data && curLen >= DWORD(qAbs(stride_) * height_)) {
                    // 从 MF 缓冲区构造 QImage 并深拷贝
                    const QImage img(data, width_, height_, stride_, QImage::Format_RGB32);
                    frame = img.copy();
                    buffer->Unlock();
                    buffer->Release();
                    sample->Release();
                    return !frame.isNull();
                }
                if (data)
                    buffer->Unlock();
                buffer->Release();
            }
            sample->Release();
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            break;
    }
    return false;
}

QString MediaFoundationCamera::name() const {
    return QStringLiteral("MediaFoundationCamera");
}

} // namespace vi
