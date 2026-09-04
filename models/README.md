# ASR Models

Download the default streaming Zipformer English transducer (INT8 encoder/joiner):

```powershell
.\scripts\download-models.ps1
```

Expected layout after download:

```text
models/
  zipformer-en/
    encoder-epoch-99-avg-1.int8.onnx
    decoder-epoch-99-avg-1.onnx
    joiner-epoch-99-avg-1.int8.onnx
    tokens.txt
```

Default bundle: `sherpa-onnx-streaming-zipformer-en-2023-06-26` (small English streaming transducer).

Model files are gitignored. Transcription works fully offline after download.
