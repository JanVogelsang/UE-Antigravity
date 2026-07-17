#!/usr/bin/env python3
import os
import time
import requests
import argparse
import sys
import wave

def load_env_file(project_root: str) -> None:
    env_path = os.path.join(project_root, ".env")
    if os.path.exists(env_path):
        with open(env_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if "=" in line:
                    key, val = line.split("=", 1)
                    val = val.strip()
                    # Strip surrounding quotes safely
                    if len(val) >= 2 and val[0] == val[-1] and val[0] in ('"', "'"):
                        val = val[1:-1]
                    os.environ[key.strip()] = val

def make_request_with_retry(method: str, url: str, max_retries: int = 3, **kwargs) -> requests.Response:
    retries = 0
    backoff = 2
    while retries < max_retries:
        try:
            res = requests.request(method, url, **kwargs)
            if res.status_code in [429, 500, 502, 503, 504]:
                print(f"Transient HTTP {res.status_code} encountered. Retrying in {backoff} seconds...", file=sys.stderr)
                time.sleep(backoff)
                retries += 1
                backoff *= 2
            else:
                return res
        except requests.RequestException as e:
            print(f"Request exception: {e}. Retrying in {backoff} seconds...", file=sys.stderr)
            time.sleep(backoff)
            retries += 1
            backoff *= 2
    return requests.request(method, url, **kwargs)

def download_file(url: str, out_path: str) -> None:
    print(f"Downloading: {url} -> {out_path}")
    res = make_request_with_retry("GET", url)
    if res.status_code != 200:
        print(f"Error downloading file: {res.text}", file=sys.stderr)
        sys.exit(1)
    with open(out_path, "wb") as f:
        f.write(res.content)

def generate_meshy_3d(prompt: str, output_dir: str) -> None:
    api_key = os.environ.get("MESHY_API_KEY")
    if not api_key:
        print("Error: MESHY_API_KEY not found in environment or .env file", file=sys.stderr)
        sys.exit(1)

    print(f"Generating Meshy 3D model for prompt: '{prompt}'")
    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json"
    }

    payload = {
        "mode": "preview",
        "prompt": prompt,
        "art_style": "realistic",
        "topology": "quad", 
        "target_polycount": 30000,
        "should_remesh": True
    }
    
    response = make_request_with_retry("POST", "https://api.meshy.ai/v2/text-to-3d", headers=headers, json=payload)
    if response.status_code not in [200, 202]:
        print(f"Error creating task: {response.text}", file=sys.stderr)
        sys.exit(1)
        
    task_id = response.json().get("result")
    print(f"Task created with ID: {task_id}. Polling for completion (max 10 minutes)...")

    # Poll for completion with a 10-minute timeout (120 iterations * 5 seconds)
    model_url = None
    texture_maps = {}
    poll_iterations = 0
    max_iterations = 120
    
    while poll_iterations < max_iterations:
        res = make_request_with_retry("GET", f"https://api.meshy.ai/v2/text-to-3d/{task_id}", headers=headers)
        if res.status_code == 200:
            result = res.json()
            status = result.get("status")
            if status == "SUCCEEDED":
                model_urls = result.get("model_urls", {})
                model_url = model_urls.get("fbx") or model_urls.get("glb")
                
                # Fetch texture URLs safely
                textures = result.get("texture_urls", [])
                if isinstance(textures, dict):
                    texture_maps = textures
                elif isinstance(textures, list) and len(textures) > 0:
                    texture_maps = textures[0]
                break
            elif status in ["FAILED", "EXPIRED"]:
                print(f"Task failed with status: {status}", file=sys.stderr)
                sys.exit(1)
            else:
                print(f"Status: {status}. Waiting 5 seconds...")
                time.sleep(5)
                poll_iterations += 1
        else:
            print(f"Error polling task: {res.text}", file=sys.stderr)
            sys.exit(1)
            
    if poll_iterations >= max_iterations:
        print("Error: Polling timed out after 10 minutes.", file=sys.stderr)
        sys.exit(1)

    if not model_url:
        print("Error: No model URL returned.", file=sys.stderr)
        sys.exit(1)
        
    os.makedirs(output_dir, exist_ok=True)
    timestamp = int(time.time())
    safe_prompt = "".join([c if c.isalnum() else "_" for c in prompt])[:20]
    
    ext = ".fbx" if "fbx" in model_url else ".glb"
    model_out = os.path.join(output_dir, f"meshy_{timestamp}_{safe_prompt}{ext}")
    download_file(model_url, model_out)
    print(f"Model saved: {model_out}")
    
    # Download textures
    for map_name, map_url in texture_maps.items():
        if map_url:
            map_out = os.path.join(output_dir, f"meshy_{timestamp}_{safe_prompt}_{map_name}.png")
            download_file(map_url, map_out)
            print(f"Texture saved ({map_name}): {map_out}")

def generate_elevenlabs_audio(prompt: str, voice_id: str, output_dir: str) -> None:
    api_key = os.environ.get("ELEVENLABS_API_KEY")
    if not api_key:
        print("Error: ELEVENLABS_API_KEY not found in environment or .env file", file=sys.stderr)
        sys.exit(1)
        
    print(f"Generating ElevenLabs audio for prompt: '{prompt}'")
    headers = {
        "Accept": "audio/wav", 
        "xi-api-key": api_key,
        "Content-Type": "application/json"
    }
    
    url = f"https://api.elevenlabs.io/v1/text-to-speech/{voice_id}?output_format=pcm_44100"
    payload = {
        "text": prompt,
        "model_id": "eleven_monolingual_v1",
        "voice_settings": {
            "stability": 0.5,
            "similarity_boost": 0.5
        }
    }
    
    res = make_request_with_retry("POST", url, json=payload, headers=headers)
    if res.status_code != 200:
        print(f"Error generating audio: {res.text}", file=sys.stderr)
        sys.exit(1)
        
    os.makedirs(output_dir, exist_ok=True)
    timestamp = int(time.time())
    safe_prompt = "".join([c if c.isalnum() else "_" for c in prompt])[:20]
    out_path = os.path.join(output_dir, f"elevenlabs_{timestamp}_{safe_prompt}.wav")
    
    try:
        with wave.open(out_path, "wb") as wav_file:
            wav_file.setnchannels(1)
            wav_file.setsampwidth(2)
            wav_file.setframerate(44100)
            wav_file.writeframes(res.content)
        print(f"Success! Saved audio to: {out_path}")
        print(out_path)
    except Exception as e:
        print(f"Error writing WAV file: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generative AI API Wrapper for UE-AgentFramework")
    parser.add_argument("--project-root", help="Root directory of the Unreal Engine project containing the .env file")
    subparsers = parser.add_subparsers(dest="command", required=True)
    
    meshy_parser = subparsers.add_parser("meshy")
    meshy_parser.add_argument("--prompt", required=True, help="Text prompt for the 3D model")
    meshy_parser.add_argument("--output_dir", help="Output directory")
    
    eleven_parser = subparsers.add_parser("elevenlabs")
    eleven_parser.add_argument("--prompt", required=True, help="Text to speak")
    eleven_parser.add_argument("--voice_id", default="21m00Tcm4TlvDq8ikWAM", help="ElevenLabs Voice ID")
    eleven_parser.add_argument("--output_dir", help="Output directory")
    
    args = parser.parse_args()
    
    # Resolve project root and load environment variables
    proj_root = args.project_root or os.environ.get("PROJECT_ROOT")
    if proj_root:
        load_env_file(proj_root)
    else:
        # Check current directory and its parent directories up to drive root
        curr = os.getcwd()
        while True:
            if os.path.exists(os.path.join(curr, ".env")):
                load_env_file(curr)
                break
            parent = os.path.dirname(curr)
            if parent == curr:
                break
            curr = parent
            
    # Resolve default output directory if not provided
    default_out = os.path.join(proj_root, "Saved", "GenAI") if proj_root else os.path.join(os.environ.get("TEMP", "/tmp"), "ue_genai")
    out_dir = args.output_dir or default_out
    
    if args.command == "meshy":
        generate_meshy_3d(args.prompt, out_dir)
    elif args.command == "elevenlabs":
        generate_elevenlabs_audio(args.prompt, args.voice_id, out_dir)
