#include "audio/synth.hpp"
#include <algorithm>
#include <cmath>
#include <string>

namespace cashyphus::audio {
namespace {
const char* effectNames[]={"push_01","push_02","push_03","push_04","push_strong","brace_start","brace_loop","brace_end","ball_roll_loop","ball_slip","stamina_low","grip_exhausted","aging","summit","death","ball_downhill","reincarnation","dialogue_tick","choice_select","walk_away"};
const char* trackNames[]={"money_loop_base","money_loop_young","money_loop_adult","money_loop_old","money_loop_final","money_loop_endless","money_loop_mechanical","freedom"};
std::string root(){
#ifdef __EMSCRIPTEN__
    return "/audio/";
#else
    return "assets/audio/";
#endif
}
// Headroom for a synthesised stand-in: a touch under the recordings' 0.65 peak,
// so a missing file never plays louder than the effect it stands in for.
constexpr float kStandinPeak=0.62f;
}
voices::Voice Synth::standinFor(Effect e){
    switch(e){
        case Effect::Push1: case Effect::Push2: case Effect::Push3: case Effect::Push4:
        case Effect::Strong:      return voices::Voice::Push;
        case Effect::BraceStart: case Effect::BraceLoop: case Effect::BraceEnd:
        case Effect::Slip:        return voices::Voice::Scrape;
        case Effect::Roll: case Effect::Downhill: return voices::Voice::Roll;
        case Effect::Stamina:     return voices::Voice::Breath;
        case Effect::Grip: case Effect::Dialogue: return voices::Voice::Rattle;
        case Effect::Aging: case Effect::Reincarnation: case Effect::Choice: return voices::Voice::Bell;
        case Effect::Summit:      return voices::Voice::Jingle;
        case Effect::Death:       return voices::Voice::Collapse;
        case Effect::WalkAway:    return voices::Voice::Birds;
        case Effect::Count:       break;
    }
    return voices::Voice::Push;
}
// Render one procedural voice into a raylib Sound. LoadSoundFromWave converts and
// copies the samples into its own audio buffer, so the staging buffer goes away.
void Synth::buildStandin(int index, Clip& c){
    const int n=static_cast<int>(voices::seconds(c.standin)*voices::RATE);
    if(n<=0) return;
    float* data=static_cast<float*>(MemAlloc(sizeof(float)*static_cast<unsigned int>(n)));
    if(data==nullptr) return;
    float peak=0.0f;
    for(int i=0;i<n;++i){data[i]=voices::sample(c.standin,i);peak=std::max(peak,std::fabs(data[i]));}
    if(peak>0.0f){const float g=kStandinPeak/peak;for(int i=0;i<n;++i)data[i]*=g;}
    Wave w{};
    w.frameCount=static_cast<unsigned int>(n);
    w.sampleRate=voices::RATE;
    w.sampleSize=32;
    w.channels=1;
    w.data=data;
    c.sound[0]=LoadSoundFromWave(w);
    MemFree(data);
    if(!IsSoundValid(c.sound[0])){c.sound[0]=Sound{};return;}
    c.loaded=true;c.voices=1;
    for(int v=1;v<kVoices;++v){const Sound alias=LoadSoundAlias(c.sound[0]);if(!IsSoundValid(alias))break;c.sound[v]=alias;c.voices=v+1;}
    TraceLog(LOG_WARNING,"CASHYPHUS: sfx/%s.wav is missing, using the synthesised voice",effectNames[index]);
}
void Synth::unloadClip(Clip& c){
    if(!c.loaded)return;
    if(c.streaming){StopMusicStream(c.stream);UnloadMusicStream(c.stream);}
    else{
        for(int v=1;v<c.voices;++v)UnloadSoundAlias(c.sound[v]);
        StopSound(c.sound[0]);
        UnloadSound(c.sound[0]);
    }
    c.loaded=false;c.voices=0;
}
void Synth::init(){
    if(ready_) return;
    // On the web the audio context is created suspended and only resumes inside a
    // user gesture, so the first attempt can legitimately find no device. main
    // keeps calling this until it succeeds, and nothing is allocated until it does.
    if (!IsAudioDeviceReady()) return;
    ready_=true;
    for(int i=0;i<static_cast<int>(Effect::Count);++i){
        auto& c=clips_[i];
        const auto path=root()+"sfx/"+effectNames[i]+".wav";
        if(FileExists(path.c_str())){
            c.streaming=i==static_cast<int>(Effect::BraceLoop) || i==static_cast<int>(Effect::Roll) || i==static_cast<int>(Effect::Downhill);
            if(c.streaming){c.stream=LoadMusicStream(path.c_str());c.loaded=IsMusicValid(c.stream);if(c.loaded)c.stream.looping=true;}
            else{
                c.sound[0]=LoadSound(path.c_str());
                if(IsSoundValid(c.sound[0])){
                    c.loaded=true;c.voices=1;
                    for(int v=1;v<kVoices;++v){const Sound alias=LoadSoundAlias(c.sound[0]);if(!IsSoundValid(alias))break;c.sound[v]=alias;c.voices=v+1;}
                }
            }
        }
        if(!c.loaded){c.standin=standinFor(static_cast<Effect>(i));buildStandin(i,c);}
    }
    for(int i=0;i<static_cast<int>(Track::Count);++i){
        auto path=root()+(i==static_cast<int>(Track::Freedom)?"ambience/":"music/")+trackNames[i]+".ogg";
        if(FileExists(path.c_str())){songs_[i].music=LoadMusicStream(path.c_str());songs_[i].loaded=IsMusicValid(songs_[i].music);if(songs_[i].loaded)songs_[i].music.looping=true;}
    }
    auto& m=songs_[static_cast<int>(current_)];if(m.loaded)PlayMusicStream(m.music);
}
void Synth::shutdown(){
    if(!ready_)return;
    for(auto& c:clips_)unloadClip(c);
    for(auto& s:songs_)if(s.loaded){StopMusicStream(s.music);UnloadMusicStream(s.music);s.loaded=false;}
    ready_=false;
}
void Synth::setVolumes(float master,float music,float effects,float dialogue,float ambience){master_=std::clamp(master,0.f,1.f);music_=std::clamp(music,0.f,1.f);effects_=std::clamp(effects,0.f,1.f);dialogue_=std::clamp(dialogue,0.f,1.f);ambience_=std::clamp(ambience,0.f,1.f);}
void Synth::play(Effect e,float pitch,float volume){
    if(!ready_)return;
    auto& c=clips_[static_cast<int>(e)];
    if(!c.loaded||c.streaming||c.voices<=0)return;
    // Take a voice that is already free, so a fast player overlaps coin tails
    // instead of restarting one voice over and over. When every voice is busy the
    // oldest is stolen, which keeps the number of simultaneous instances fixed
    // however hard the player mashes the key.
    int pick=c.next;
    for(int v=0;v<c.voices;++v){const int idx=(c.next+v)%c.voices;if(!IsSoundPlaying(c.sound[idx])){pick=idx;break;}}
    c.next=(pick+1)%c.voices;
    SetSoundPitch(c.sound[pick],std::clamp(pitch,.55f,1.5f));
    const float category=e==Effect::Dialogue?dialogue_:effects_;
    SetSoundVolume(c.sound[pick],master_*category*std::clamp(volume,0.f,1.f));
    PlaySound(c.sound[pick]);
}
void Synth::loop(Effect e,float target,float dt){
    if(!ready_)return;
    auto& c=clips_[static_cast<int>(e)];if(!c.loaded||!c.streaming)return;
    c.gain=std::clamp(c.gain+(target>c.gain?1.f:-1.f)*dt*3.f,0.f,1.f);
    if(c.gain>0.01f){
        if(!IsMusicStreamPlaying(c.stream))PlayMusicStream(c.stream);
        SetMusicVolume(c.stream,master_*effects_*c.gain);
    }else if(IsMusicStreamPlaying(c.stream))StopMusicStream(c.stream);
}
void Synth::select(Track t){wanted_=t;}
void Synth::update(float dt){
    if(!ready_)return;
    if(current_!=wanted_){
        songGain_=std::max(0.f,songGain_-dt*2.f);
        if(songGain_<=0){musicActual_=0;auto& old=songs_[static_cast<int>(current_)];if(old.loaded)StopMusicStream(old.music);current_=wanted_;auto& next=songs_[static_cast<int>(current_)];if(next.loaded)PlayMusicStream(next.music);}
    }else songGain_=std::min(1.f,songGain_+dt*1.2f);
    for(auto& c:clips_)if(c.loaded&&c.streaming&&IsMusicStreamPlaying(c.stream))UpdateMusicStream(c.stream);
    auto& s=songs_[static_cast<int>(current_)];
    float desired=master_*(current_==Track::Freedom?ambience_:music_)*songGain_;
    musicActual_+=std::clamp(desired-musicActual_,-dt*0.9f,dt*0.9f);
    if(s.loaded){UpdateMusicStream(s.music);SetMusicVolume(s.music,musicActual_);}
}
}
