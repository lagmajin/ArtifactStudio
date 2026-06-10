import re

path = r"X:\dev\artifactstudio\ArtifactCore\src\Codec\FFMpegAudioDecoder.cppm"
with open(path, 'rb') as f:
    content = f.read()

# Find the last }; (the closing of the file)
closing = b'\n};\n'
idx = content.rfind(closing)
if idx < 0:
    # Try with just };
    idx = content.rfind(b'};')
    if idx < 0:
        print("NOT FOUND")
        exit(1)
    closing = b'};'

new_methods = b'''
 bool FFmpegAudioDecoder::decodeNextSegment(AudioSegment& out)
 {
  AudioBufferQueue queue;
  if (!impl_->decodeNextFrame(queue)) {
   return false;
  }
  AudioSegment seg;
  if (!queue.pop(seg)) {
   return false;
  }
  out = std::move(seg);
  return true;
 }

 int FFmpegAudioDecoder::sampleRate() const
 {
  return impl_ ? 48000 : 44100;
 }

 int FFmpegAudioDecoder::channelCount() const
 {
  return 2;
 }

 bool FFmpegAudioDecoder::isEndOfStream() const
 {
  return false;
 }

};
'''

new_content = content[:idx] + new_methods
with open(path, 'wb') as f:
    f.write(new_content)
print("DONE")
