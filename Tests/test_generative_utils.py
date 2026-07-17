import os
import sys
import pytest
import wave
from pathlib import Path
from unittest.mock import patch, MagicMock

# Ensure UnrealEngine folder is in sys.path
sys.path.append(str(Path(__file__).resolve().parent.parent / "UnrealEngine"))

from ExternalServer.src import generative_utils

@patch.dict(os.environ, {"MESHY_API_KEY": "test_meshy_key"})
def test_generate_meshy_3d_success(tmp_path):
    temp_output_dir = str(tmp_path)
    
    # Mock POST /v2/text-to-3d response
    mock_post_res = MagicMock()
    mock_post_res.status_code = 202
    mock_post_res.json.return_value = {"result": "task_12345"}
    
    # Mock GET /v2/text-to-3d/task_12345 response (status = SUCCEEDED)
    mock_get_res = MagicMock()
    mock_get_res.status_code = 200
    mock_get_res.json.return_value = {
        "status": "SUCCEEDED",
        "model_urls": {
            "fbx": "https://api.meshy.ai/assets/model.fbx"
        },
        "texture_urls": [
            {
                "base_color": "https://api.meshy.ai/assets/base_color.png",
                "normal": "https://api.meshy.ai/assets/normal.png"
            }
        ]
    }
    
    # Mock file download responses
    mock_dl_fbx = MagicMock()
    mock_dl_fbx.status_code = 200
    mock_dl_fbx.content = b"FBX_CONTENT"
    
    mock_dl_png = MagicMock()
    mock_dl_png.status_code = 200
    mock_dl_png.content = b"PNG_CONTENT"
    
    def mock_request(method, url, **kwargs):
        if method == "POST" and "text-to-3d" in url:
            return mock_post_res
        elif method == "GET" and "task_12345" in url:
            return mock_get_res
        elif method == "GET" and "model.fbx" in url:
            return mock_dl_fbx
        elif method == "GET" and "base_color.png" in url:
            return mock_dl_png
        elif method == "GET" and "normal.png" in url:
            return mock_dl_png
        return MagicMock(status_code=404)
        
    with patch("ExternalServer.src.generative_utils.requests.request", side_effect=mock_request):
        generative_utils.generate_meshy_3d("wooden crate", temp_output_dir)
        
        # Check files are generated
        files = os.listdir(temp_output_dir)
        assert any(f.endswith(".fbx") for f in files)
        assert any(f.endswith("_base_color.png") for f in files)
        assert any(f.endswith("_normal.png") for f in files)

@patch.dict(os.environ, {"ELEVENLABS_API_KEY": "test_eleven_key"})
def test_generate_elevenlabs_audio_success(tmp_path):
    temp_output_dir = str(tmp_path)
    
    # Mock text-to-speech response
    mock_pcm_bytes = b"\x00\x00" * 44100 # 1 second of silence in 16-bit mono 44.1kHz PCM
    
    mock_res = MagicMock()
    mock_res.status_code = 200
    mock_res.content = mock_pcm_bytes
    
    with patch("ExternalServer.src.generative_utils.requests.request", return_value=mock_res):
        generative_utils.generate_elevenlabs_audio("hello world", "voice_id_123", temp_output_dir)
        
        files = os.listdir(temp_output_dir)
        wav_files = [f for f in files if f.endswith(".wav")]
        assert len(wav_files) == 1
        
        # Verify it's a valid WAV file
        wav_path = os.path.join(temp_output_dir, wav_files[0])
        with wave.open(wav_path, "rb") as w:
            assert w.getnchannels() == 1
            assert w.getsampwidth() == 2
            assert w.getframerate() == 44100
            assert w.getnframes() == 44100

@patch.dict(os.environ, {"MESHY_API_KEY": "test_meshy_key"})
def test_generate_meshy_3d_retry(tmp_path):
    temp_output_dir = str(tmp_path)
    
    # Test that transient 429 retries successfully
    mock_post_res = MagicMock()
    mock_post_res.status_code = 202
    mock_post_res.json.return_value = {"result": "task_retry"}
    
    # First poll returns 429, second succeeds
    mock_get_429 = MagicMock()
    mock_get_429.status_code = 429
    
    mock_get_succeed = MagicMock()
    mock_get_succeed.status_code = 200
    mock_get_succeed.json.return_value = {
        "status": "SUCCEEDED",
        "model_urls": {"fbx": "https://api.meshy.ai/assets/model.fbx"},
        "texture_urls": []
    }
    
    mock_dl = MagicMock()
    mock_dl.status_code = 200
    mock_dl.content = b"FBX"
    
    calls = []
    def mock_request(method, url, **kwargs):
        calls.append((method, url))
        if method == "POST" and "text-to-3d" in url:
            return mock_post_res
        elif method == "GET" and "task_retry" in url:
            if len(calls) == 2: # first get call
                return mock_get_429
            return mock_get_succeed
        elif method == "GET" and "model.fbx" in url:
            return mock_dl
        return MagicMock(status_code=404)
        
    with patch("ExternalServer.src.generative_utils.requests.request", side_effect=mock_request), \
         patch("time.sleep") as mock_sleep: # Mock sleep to speed up test
        generative_utils.generate_meshy_3d("wooden crate", temp_output_dir)
        assert mock_sleep.call_count == 1
        assert any(f.endswith(".fbx") for f in os.listdir(temp_output_dir))

def test_load_env_file_strips_quotes(tmp_path):
    # Test that custom env parser strips surrounding quotes from variables
    env_file = tmp_path / ".env"
    env_file.write_text('MESHY_API_KEY="my_quoted_key"\nELEVENLABS_API_KEY=\'another_key\'\nNORMAL_KEY=unquoted_val', encoding="utf-8")
    
    with patch.dict(os.environ, {}):
        generative_utils.load_env_file(str(tmp_path))
        assert os.environ.get("MESHY_API_KEY") == "my_quoted_key"
        assert os.environ.get("ELEVENLABS_API_KEY") == "another_key"
        assert os.environ.get("NORMAL_KEY") == "unquoted_val"
