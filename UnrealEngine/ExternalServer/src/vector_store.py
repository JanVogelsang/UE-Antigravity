import os
import logging
import threading
import urllib.request
import zipfile
import shutil
from typing import List, Dict, Any

logger = logging.getLogger("CppAstMcpServer.VectorStore")

# Disable ChromaDB telemetry before importing it
os.environ["CHROMA_TELEMETRY"] = "false"
os.environ["ANONYMIZED_TELEMETRY"] = "False"

# Lazy load chromadb to prevent slow MCP handshake on startup
chromadb = None
ONNXMiniLM_L6_V2 = None

_client = None
_collection = None
_bp_collection = None
_initialized = False
_is_initializing = False
_is_downloading = False
_download_status = ""
_loaded_version = None
_init_lock = threading.Lock()

def _ensure_imports():
    global chromadb, ONNXMiniLM_L6_V2
    if chromadb is None:
        try:
            import chromadb as _chromadb
            from chromadb.utils.embedding_functions import ONNXMiniLM_L6_V2 as _ONNXMiniLM_L6_V2
            chromadb = _chromadb
            ONNXMiniLM_L6_V2 = _ONNXMiniLM_L6_V2
        except ImportError as e:
            logger.error(f"Failed to import chromadb or ONNX runtime: {e}")
            raise

def get_loaded_version() -> str:
    with _init_lock:
        return _loaded_version

def download_and_extract_db(ue_version: str, persist_directory: str) -> bool:
    """
    Downloads the pre-compiled ChromaDB ZIP file for the specified Unreal version
    and extracts it to the persist_directory.
    """
    base_url = os.environ.get(
        "UE_AGENTFRAMEWORK_VECTOR_DB_URL",
        "https://github.com/JanVogelsang/UE-Antigravity/releases/download/vector-dbs"
    )
    download_url = f"{base_url}/vector_db_{ue_version}.zip"
    
    server_dir = os.path.dirname(persist_directory)
    temp_zip = os.path.join(server_dir, f"temp_vector_db_{ue_version}.zip")
    
    logger.info(f"Downloading pre-compiled vector database from {download_url} to {temp_zip}...")
    try:
        os.makedirs(server_dir, exist_ok=True)
        
        req = urllib.request.Request(
            download_url,
            headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'}
        )
        with urllib.request.urlopen(req) as response:
            with open(temp_zip, 'wb') as out_file:
                shutil.copyfileobj(response, out_file)
                
        logger.info(f"Extracting vector database to {persist_directory}...")
        os.makedirs(persist_directory, exist_ok=True)
        with zipfile.ZipFile(temp_zip, 'r') as zip_ref:
            zip_ref.extractall(persist_directory)
            
        if os.path.exists(temp_zip):
            os.remove(temp_zip)
            
        # Flatten structure if the zip contains a nested folder of the same name
        nested_dir = os.path.join(persist_directory, f"vector_db_{ue_version}")
        if os.path.exists(nested_dir) and os.path.isdir(nested_dir):
            logger.info(f"Flattening nested directory {nested_dir}...")
            for item in os.listdir(nested_dir):
                s = os.path.join(nested_dir, item)
                d = os.path.join(persist_directory, item)
                if os.path.exists(d):
                    if os.path.isdir(d):
                        shutil.rmtree(d)
                    else:
                        os.remove(d)
                shutil.move(s, d)
            os.rmdir(nested_dir)
            
        logger.info(f"Successfully downloaded and extracted vector database for UE {ue_version}.")
        return True
    except Exception as e:
        logger.error(f"Failed to download/extract vector database for UE {ue_version}: {e}")
        if os.path.exists(temp_zip):
            try:
                os.remove(temp_zip)
            except Exception:
                pass
        return False

def initialize_db(persist_directory: str, ue_version: str) -> bool:
    """
    Initializes the local persistent ChromaDB client and gets the unreal_docs collection.
    Performs the initialization in a background thread to prevent slow startup blocking the MCP handshake.
    If the database folder does not exist, automatically downloads it first.
    """
    global _client, _collection, _bp_collection, _initialized, _is_initializing, _is_downloading, _download_status, _loaded_version
    
    with _init_lock:
        if _initialized and _loaded_version == ue_version:
            return True
        if _is_initializing:
            return True
        _is_initializing = True
        _initialized = False
        _client = None
        _collection = None
        _bp_collection = None
        _loaded_version = ue_version

    def do_init():
        global _client, _collection, _bp_collection, _initialized, _is_initializing, _is_downloading, _download_status
        try:
            _ensure_imports()
            
            if not os.path.exists(persist_directory):
                logger.info(f"Vector database directory {persist_directory} not found. Attempting auto-download...")
                with _init_lock:
                    _is_downloading = True
                    _download_status = f"Downloading vector database for UE {ue_version}..."
                
                success = download_and_extract_db(ue_version, persist_directory)
                
                with _init_lock:
                    _is_downloading = False
                    _download_status = ""
                
                if not success:
                    logger.warning("Auto-download failed. An empty database will be initialized.")
            
            settings = chromadb.Settings(anonymized_telemetry=False)
            client = chromadb.PersistentClient(path=persist_directory, settings=settings)
            
            embedding_fn = ONNXMiniLM_L6_V2()
            collection = client.get_or_create_collection(
                name="unreal_docs",
                embedding_function=embedding_fn
            )
            
            bp_collection = client.get_or_create_collection(
                name="blueprints",
                embedding_function=embedding_fn
            )
            
            with _init_lock:
                _client = client
                _collection = collection
                _bp_collection = bp_collection
                _initialized = True
                _is_initializing = False
            logger.info(f"Successfully initialized ChromaDB from {persist_directory} with {get_document_count()} documents.")
        except Exception as e:
            logger.error(f"Failed to initialize ChromaDB in background thread: {e}")
            with _init_lock:
                _is_initializing = False
                _is_downloading = False
                _download_status = ""

    thread = threading.Thread(target=do_init, daemon=True)
    thread.start()
    return True

def wait_for_initialization(timeout: float = 10.0) -> bool:
    """
    Blocks until initialization completes or timeout is reached.
    Returns True if initialized, False otherwise.
    """
    import time
    start_time = time.time()
    while time.time() - start_time < timeout:
        with _init_lock:
            if _initialized:
                return True
            if not _is_initializing:
                return False
        time.sleep(0.1)
    return False
def get_document_count() -> int:
    """Returns the total number of documents in the unreal_docs collection."""
    with _init_lock:
        if _collection is not None:
            try:
                return _collection.count()
            except Exception:
                pass
    return 0

def upsert_documents(documents: List[str], metadatas: List[Dict[str, Any]], ids: List[str]) -> bool:
    """
    Upserts documents into the unreal_docs collection.
    """
    with _init_lock:
        collection_ref = _collection
    if collection_ref is None:
        return False
    try:
        _ensure_imports()
        collection_ref.upsert(
            ids=ids,
            documents=documents,
            metadatas=metadatas
        )
        return True
    except Exception as e:
        logger.error(f"Failed to upsert documents: {e}")
        return False

def semantic_search(query: str, n_results: int = 5) -> List[Dict[str, Any]]:
    """
    Queries the unreal_docs collection for semantic similarity.
    """
    if not query or not query.strip():
        return []

    with _init_lock:
        is_ready = _initialized
        is_loading = _is_initializing
        is_downloading = _is_downloading
        download_status = _download_status
        collection_ref = _collection

    if not is_ready:
        if is_downloading or is_loading:
            status_msg = download_status if is_downloading else "The vector database is currently initializing (loading model weights)."
            logger.info("Semantic search query received while ChromaDB is still initializing/downloading.")
            return [{
                "title": "Search Database Status",
                "content": f"{status_msg} Please retry query '{query}' in a few seconds.",
                "similarity_score": 0.0,
                "source": "System"
            }]
        logger.warning("Query attempted but vector database is not initialized.")
        return []

    try:
        _ensure_imports()
        results = collection_ref.query(
            query_texts=[query],
            n_results=n_results
        )
        
        formatted_results = []
        ids = results.get("ids", [[]])[0]
        distances = results.get("distances", [[]])[0]
        metadatas = results.get("metadatas", [[]])[0]
        documents = results.get("documents", [[]])[0]
        
        for i in range(len(ids)):
            metadata = metadatas[i] if i < len(metadatas) and metadatas[i] else {}
            distance = distances[i] if i < len(distances) else 1.0
            doc = documents[i] if i < len(documents) else ""
            
            similarity = max(0.0, round(1.0 - (distance / 2.0), 4))
            
            formatted_results.append({
                "title": metadata.get("title", f"Chunk {ids[i]}"),
                "content": doc,
                "similarity_score": similarity,
                "source": metadata.get("source", "Unknown")
            })
            
        return formatted_results
    except Exception as e:
        logger.error(f"Error during semantic search query: {e}")
        return []

def upsert_blueprint(asset_path: str, json_data: dict, timestamp: float = 0.0) -> bool:
    """
    Upserts a Blueprint summary into the blueprints collection in a chunked, semantic manner.
    """
    collection_ref = _bp_collection
    if collection_ref is None:
        return False
        
    try:
        _ensure_imports()
        
        # 1. Clean up old chunks for this blueprint to avoid orphans
        try:
            collection_ref.delete(where={"asset_path": asset_path})
        except Exception as e:
            logger.debug(f"No existing chunks to delete for {asset_path}: {e}")
            
        # 2. Build chunks
        ids = []
        documents = []
        metadatas = []
        
        # Base chunk (parent class, variables, components)
        parent_class = json_data.get("parent_class", "None")
        vars_list = json_data.get("variables", [])
        comps_list = json_data.get("components", [])
        
        vars_str = "\n".join([f"- {v.get('name')}: {v.get('type')} (Category: {v.get('category')})" for v in vars_list])
        comps_str = "\n".join([f"- {c.get('name')}: {c.get('class')} (Parent: {c.get('parent')})" for c in comps_list])
        
        base_doc = f"Blueprint: {asset_path}\nParent Class: {parent_class}\n\nVariables:\n{vars_str}\n\nComponents:\n{comps_str}"
        
        ids.append(f"{asset_path}_base")
        documents.append(base_doc)
        metadatas.append({
            "asset_path": asset_path,
            "chunk_type": "base",
            "timestamp": timestamp
        })
        
        # Graph chunks (Functions, Macros, EventGraphs)
        graphs = json_data.get("graphs", [])
        for g in graphs:
            graph_name = g.get("name", "Unknown")
            logic = g.get("logic_summary", "")
            if not logic.strip():
                continue
            
            graph_doc = f"Blueprint: {asset_path}\nGraph: {graph_name}\n\nLogic Summary:\n{logic}"
            ids.append(f"{asset_path}_graph_{graph_name}")
            documents.append(graph_doc)
            metadatas.append({
                "asset_path": asset_path,
                "chunk_type": "graph",
                "graph_name": graph_name,
                "timestamp": timestamp
            })
            
        # Upsert chunks if any exist
        if ids:
            collection_ref.upsert(
                ids=ids,
                documents=documents,
                metadatas=metadatas
            )
        return True
    except Exception as e:
        logger.error(f"Failed to upsert blueprint chunks for {asset_path}: {e}")
        return False

def search_similar_blueprints(query: str, n_results: int = 3) -> List[Dict[str, Any]]:
    """
    Queries the blueprints collection for semantic similarity.
    """
    if not query or not query.strip():
        return []

    with _init_lock:
        is_ready = _initialized
        is_loading = _is_initializing
        collection_ref = _bp_collection

    if not is_ready:
        if is_loading:
            return [{"error": "Initializing ChromaDB. Try again."}]
        return []
        
    try:
        _ensure_imports()
        results = collection_ref.query(
            query_texts=[query],
            n_results=n_results
        )
        
        formatted_results = []
        ids = results.get("ids", [[]])[0]
        distances = results.get("distances", [[]])[0]
        metadatas = results.get("metadatas", [[]])[0]
        documents = results.get("documents", [[]])[0]
        
        for i in range(len(ids)):
            metadata = metadatas[i] if i < len(metadatas) and metadatas[i] else {}
            distance = distances[i] if i < len(distances) else 1.0
            doc = documents[i] if i < len(documents) else ""
            
            similarity = max(0.0, round(1.0 - (distance / 2.0), 4))
            
            formatted_results.append({
                "asset_path": metadata.get("asset_path", ids[i]),
                "chunk_type": metadata.get("chunk_type", "unknown"),
                "graph_name": metadata.get("graph_name", ""),
                "logic_summary": doc,
                "similarity_score": similarity
            })
            
        return formatted_results
    except Exception as e:
        logger.error(f"Error querying blueprint vector database: {e}")
        return []

def get_indexed_blueprints() -> Dict[str, float]:
    """
    Returns a dictionary of asset_path -> timestamp for all currently indexed blueprints.
    """
    collection_ref = _bp_collection
    if collection_ref is None:
        return {}
    try:
        _ensure_imports()
        results = collection_ref.get(
            include=["metadatas"]
        )
        metadatas = results.get("metadatas", [])
        
        asset_timestamps = {}
        for meta in metadatas:
            if not meta: continue
            asset_path = meta.get("asset_path")
            timestamp = meta.get("timestamp", 0.0)
            if asset_path:
                asset_timestamps[asset_path] = max(asset_timestamps.get(asset_path, 0.0), float(timestamp))
        return asset_timestamps
    except Exception as e:
        logger.error(f"Failed to get indexed blueprints: {e}")
        return {}
