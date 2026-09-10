# External artifacts and integrity

Large images, NPU models, vendor firmware and proprietary tools are not stored
in Git history. This avoids GitHub size limits and prevents redistribution of
license-restricted material.

## Latest development image

```text
name: openvela-a733-cubie-a7z-sd-usb-camera-v65-candidate.img
size: 2147483648 bytes
sha256: f2f126c414b24e2e77f6fd8c239b5c7e2a0b2bc03847f86051268ca175266683
```

This is a candidate image because USB camera enumeration is incomplete. The
kernel embedded in it has:

```text
sha256: 583fad8f85de3a0655c16b40a464d1e36866245e292dde39fb0b699c734908fb
```

A final contest release must publish a freshly rebuilt, verified image as a
GitHub Release asset and update this file with its URL, size and SHA-256.

## Prepared NPU packages used during development

These values allow an evaluator with lawful access to the official model and
VIPLite tools to verify the exact inputs:

| Artifact | Size | SHA-256 |
| --- | ---: | --- |
| `lenet.a7pm` | 441856 | `1e65fc966d5aabbda46f573e16c41c63e6ca3019d241feea12190b1cb38aa027` |
| `yolov5s-zero.a7pm` | 11239104 | `3feb08593d743bb5ca239110013954fe08ad8a864c81fd719b65b2630dbc4472` |
| `yolov8n-pcq-v3.a7pm` | 8017024 | `3c2891ef77ce9a44dd02cd32bdd7db5ec92d14185014e6de8db5803a6cbecce2` |
| dog RGB input | 1228800 | `07a1654c992b1a2e38ccc595a571994889134df9594eef09b5a85d70f4820c60` |
| black RGB input | 1228800 | `3630e065eb7b4540fbab11dbfd2619e8500f211b9c404380a1867fdc44b77c0c` |

A7PM packages contain a checked VIP virtual-address layout captured from an
authorized official Linux VIPLite prepare/run. They do not replace or decode
Allwinner's proprietary `.nb` compiler/linker.

## Runtime-only data

The following belong on the board data partition, not in source control:

- FCU760K D80-U02 firmware obtained from the official BSP;
- HTTPS CA certificates;
- generated SSH host keys;
- local Wi-Fi credentials and service configuration;
- A7PM/model files whose redistribution terms permit local use;
- user data, captured frames and inference outputs.

No password, private key or API token is required to build this repository.
