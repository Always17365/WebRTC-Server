//
//  IAudioRenderer.h
//  RtmpClient
//
//  Created by Max on 2017/6/15.
//  Copyright © 2017年 net.qdating. All rights reserved.
//

#ifndef IAUDIORENDERER_H
#define IAUDIORENDERER_H

namespace mediaserver {
    class AudioRenderer {
    public:
        virtual ~AudioRenderer(){};
        virtual void RenderAudioFrame(void* audioFrame) = 0;
        virtual bool Start() = 0;
        virtual void Stop() = 0;
        virtual void Reset() = 0;
        virtual bool GetMute() = 0;
        virtual void SetMute(bool isMute) = 0;
    };
} /* namespace mediaserver */

#endif /* IAUDIORENDERER_H */
