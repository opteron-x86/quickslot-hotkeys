#pragma once
namespace RE {class InputEvent;}
struct ID3D11DeviceContext;
namespace quickslot {
bool initializeVROverlay();
bool vrOverlayReady();
bool vrOtherOverlayOpen();
void feedVROverlay(RE::InputEvent*);

bool beginVROverlay(bool editorOpen);
void finishVROverlay(ID3D11DeviceContext*,bool editorOpen);
void clearVROverlay(ID3D11DeviceContext*);
void vrEditorResult(bool open);
bool vrEditorWanted();
}
