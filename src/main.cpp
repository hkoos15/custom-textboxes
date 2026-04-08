#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
using namespace geode::prelude;

// ── Character definitions ─────────────────────────────────────────────────────
struct CharacterDef {
    std::string id;
    std::string displayName;
    std::vector<int> faceIcons;
    ccColor3B portraitColor;
};

static const std::vector<CharacterDef> CHAR_DEFS = {
    {"Scratch",    "Scratch",            {8,9,10,11,12,13,14},    {60,150,140}},
    {"Shopkeeper", "Shopkeeper",         {5,6,29,30,31,32},       {200, 80, 80}},
    {"Potbor",     "Potbor",             {19,20,21,22,23,24,25},  {200,150,  0}},
    {"Spooky",     "Spooky",             {1},                     {120, 40, 40}},
    {"Gatekeeper", "Gatekeeper",         {3,7},                   { 80, 50, 20}},
    {"Keymaster",  "The Keymaster",      {2,4,17,18},             { 40, 40,120}},
    {"Mechanic",   "Mechanic",           {37,38,39,44,45,46},     {120, 60,180}},
    {"DiamondShop","Diamond Shopkeeper", {40,41,42,43,47},        {  0,160,210}},
};

// ── All available GD fonts ────────────────────────────────────────────────────
static const std::vector<std::string> AVAILABLE_FONTS = {
    "bigFont.fnt", "chatFont.fnt", "goldFont.fnt",
    "gjFont01.fnt","gjFont02.fnt","gjFont03.fnt","gjFont04.fnt","gjFont05.fnt",
    "gjFont06.fnt","gjFont07.fnt","gjFont08.fnt","gjFont09.fnt","gjFont10.fnt",
    "gjFont11.fnt","gjFont12.fnt","gjFont13.fnt","gjFont14.fnt","gjFont15.fnt",
    "gjFont16.fnt","gjFont17.fnt","gjFont18.fnt","gjFont19.fnt","gjFont20.fnt",
    "gjFont21.fnt","gjFont22.fnt","gjFont23.fnt","gjFont24.fnt","gjFont25.fnt",
    "gjFont26.fnt","gjFont27.fnt","gjFont28.fnt","gjFont29.fnt","gjFont30.fnt",
    "gjFont31.fnt","gjFont32.fnt","gjFont33.fnt","gjFont34.fnt","gjFont35.fnt",
    "gjFont36.fnt","gjFont37.fnt","gjFont38.fnt","gjFont39.fnt","gjFont40.fnt",
    "gjFont41.fnt","gjFont42.fnt","gjFont43.fnt","gjFont44.fnt","gjFont45.fnt",
    "gjFont46.fnt","gjFont47.fnt","gjFont48.fnt","gjFont49.fnt","gjFont50.fnt",
    "gjFont51.fnt","gjFont52.fnt","gjFont53.fnt","gjFont54.fnt","gjFont55.fnt",
    "gjFont56.fnt","gjFont57.fnt","gjFont58.fnt","gjFont59.fnt",
};

static const std::string DEFAULT_FONT = "chatFont.fnt";

// Strip ".fnt" for display (e.g. "gjFont17.fnt" → "gjFont17")
static std::string fontDisplayName(const std::string& f) {
    if (f.size() > 4 && f.substr(f.size()-4) == ".fnt")
        return f.substr(0, f.size()-4);
    return f;
}

// ── Global state ──────────────────────────────────────────────────────────────
static std::map<std::string, std::string> customTexts;
static std::map<std::string, enumKeyCodes> customKeybinds;
static std::map<std::string, int>         customFaces;  // raw dialogIcon number
static std::map<std::string, std::string> customFonts;  // font file name
static bool g_capturingKeybind = false;
static std::string g_capturingForChar = "";
static CCLabelBMFont* g_keybindLabel = nullptr;

// ── Helpers ───────────────────────────────────────────────────────────────────
static std::string getKeyName(enumKeyCodes key) {
    if (key == KEY_F1) return "F1";  if (key == KEY_F2) return "F2";
    if (key == KEY_F3) return "F3";  if (key == KEY_F4) return "F4";
    if (key == KEY_F5) return "F5";  if (key == KEY_F6) return "F6";
    if (key == KEY_F7) return "F7";  if (key == KEY_F8) return "F8";
    if (key >= 65 && key <= 90) return std::string(1, (char)key);
    if (key >= 48 && key <= 57) return std::string(1, (char)key);
    return "Key" + std::to_string((int)key);
}

static CCSprite* tryLoadIcon(int iconNum) {
    char name[64];
    snprintf(name, sizeof(name), "dialogIcon_%03d.png", iconNum);
    auto* spr = CCSprite::create(name);
    if (spr && spr->getContentSize().width > 4 && spr->getContentSize().height > 4)
        return spr;
    return nullptr;
}

static int getFaceIconNum(const std::string& id) {
    auto it = customFaces.find(id);
    if (it != customFaces.end()) return it->second;
    for (auto& def : CHAR_DEFS)
        if (def.id == id && !def.faceIcons.empty()) return def.faceIcons[0];
    return 1;
}

static std::string getFont(const std::string& id) {
    auto it = customFonts.find(id);
    if (it != customFonts.end()) return it->second;
    return DEFAULT_FONT;
}

static const CharacterDef* findDef(const std::string& id) {
    for (auto& def : CHAR_DEFS) if (def.id == id) return &def;
    return nullptr;
}

static void setAllMenusEnabled(bool enabled, CCNode* node) {
    if (!node || !node->getChildren()) return;
    for (auto* child : CCArrayExt<CCNode*>(node->getChildren())) {
        if (auto menu = typeinfo_cast<CCMenu*>(child)) menu->setEnabled(enabled);
        setAllMenusEnabled(enabled, child);
    }
}
static void setGameMenusEnabled(bool enabled) {
    setAllMenusEnabled(enabled, CCDirector::get()->getRunningScene());
}

// ── CharacterDialogLayer ──────────────────────────────────────────────────────
class CharacterDialogLayer : public CCLayer {
public:
    CCRect m_panelRect;

    static CharacterDialogLayer* create(const std::string& charId) {
        auto ret = new CharacterDialogLayer();
        if (ret->initWithChar(charId)) { ret->autorelease(); return ret; }
        delete ret; return nullptr;
    }

    bool initWithChar(const std::string& charId) {
        if (!CCLayer::init()) return false;
        auto* def = findDef(charId);
        if (!def) return false;

        auto winSize = CCDirector::get()->getWinSize();
        float cx = winSize.width / 2, cy = winSize.height / 2;

        auto dim = CCLayerColor::create({0,0,0,130}, winSize.width, winSize.height);
        this->addChild(dim, -1);

        const float boxW = 490, boxH = 170;
        m_panelRect = CCRect(cx - boxW/2, cy - boxH/2, boxW, boxH);

        auto panel = CCScale9Sprite::create("GJ_square02.png", {0,0,80,80});
        panel->setContentSize({boxW, boxH});
        panel->setPosition({cx, cy});
        panel->setColor({20, 45, 110});
        this->addChild(panel);

        // Portrait box
        const float portSize = 120;
        const float portX = cx - boxW/2 + portSize/2 + 10;
        const float portY = cy + 12;

        auto portBg = CCScale9Sprite::create("GJ_square02.png", {0,0,80,80});
        portBg->setContentSize({portSize, portSize});
        portBg->setPosition({portX, portY});
        portBg->setColor(def->portraitColor);
        this->addChild(portBg);

        int iconNum = getFaceIconNum(charId);
        auto* portrait = tryLoadIcon(iconNum);
        if (portrait) {
            float s = (portSize - 8) / std::max(
                portrait->getContentSize().width,
                portrait->getContentSize().height);
            portrait->setScale(s);
            portrait->setPosition({portX, portY});
            this->addChild(portrait);
        }

        // Text area — uses the character's chosen font
        const float textX    = portX + portSize/2 + 14;
        const float textMaxW = cx + boxW/2 - textX - 10;
        std::string chosenFont = getFont(charId);

        auto nameLbl = CCLabelBMFont::create(def->displayName.c_str(), "bigFont.fnt");
        nameLbl->setScale(0.65f);
        nameLbl->setAnchorPoint({0, 0.5f});
        nameLbl->setPosition({textX, cy + 42});
        this->addChild(nameLbl);

        std::string body = "(no text saved)";
        auto tit = customTexts.find(charId);
        if (tit != customTexts.end() && !tit->second.empty()) body = tit->second;

        const float bodyScale = 0.72f;
        auto bodyLbl = CCLabelBMFont::create(body.c_str(), chosenFont.c_str());
        bodyLbl->setWidth(textMaxW / bodyScale);
        bodyLbl->setScale(bodyScale);
        bodyLbl->setAnchorPoint({0, 0.5f});
        bodyLbl->setPosition({textX, cy + 8});
        this->addChild(bodyLbl);

        // OK button inside panel
        auto okMenu = CCMenu::create();
        okMenu->setPosition({cx, cy - boxH/2 + 22});
        this->addChild(okMenu);

        auto okBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("OK","bigFont.fnt","GJ_button_01.png",0.85f),
            this, menu_selector(CharacterDialogLayer::onOK));
        okMenu->addChild(okBtn);

        this->setTouchEnabled(true);
        return true;
    }

    void registerWithTouchDispatcher() override {
        CCDirector::get()->getTouchDispatcher()
            ->addTargetedDelegate(this, -500, true);
    }
    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        if (m_panelRect.containsPoint(touch->getLocation())) return false;
        return true;
    }
    void onOK(CCObject*) { this->removeFromParentAndCleanup(true); }
};

// ── TextInputLayer ─────────────────────────────────────────────────────────────
class TextInputLayer : public CCLayer, public TextInputDelegate {
public:
    int charIdx       = 0;
    int faceIconNum   = 1;
    int faceListIdx   = 0;
    int fontListIdx   = 0;  // index into AVAILABLE_FONTS

    CCTextInputNode* inputNode      = nullptr;
    CCLabelBMFont*   keybindLabel   = nullptr;
    CCLabelBMFont*   keybindHint    = nullptr;
    CCLabelBMFont*   faceNumLabel   = nullptr;
    CCLabelBMFont*   fontPreviewLbl = nullptr; // shows "Hello!" in chosen font
    CCLabelBMFont*   fontNameLbl    = nullptr; // shows font name in chatFont
    CCSprite*        facePreviewSpr = nullptr;
    CCNode*          facePreviewBox = nullptr;
    CCScale9Sprite*  m_panel        = nullptr;
    CCLayer*         m_parentPopup  = nullptr;

    static TextInputLayer* create(int idx, CCLayer* parentPopup) {
        auto ret = new TextInputLayer();
        ret->charIdx       = idx;
        ret->m_parentPopup = parentPopup;
        if (ret->init()) { ret->autorelease(); return ret; }
        delete ret; return nullptr;
    }

    const CharacterDef& getDef() const { return CHAR_DEFS[charIdx]; }

    bool init() override {
        if (!CCLayer::init()) return false;
        auto winSize = CCDirector::get()->getWinSize();
        float cx = winSize.width / 2, cy = winSize.height / 2;

        // Restore saved face
        int savedIcon = getFaceIconNum(getDef().id);
        auto& icons = getDef().faceIcons;
        faceListIdx = 0;
        for (int i = 0; i < (int)icons.size(); i++)
            if (icons[i] == savedIcon) { faceListIdx = i; break; }
        faceIconNum = icons.empty() ? 1 : icons[faceListIdx];

        // Restore saved font
        std::string savedFont = getFont(getDef().id);
        fontListIdx = 0;
        for (int i = 0; i < (int)AVAILABLE_FONTS.size(); i++)
            if (AVAILABLE_FONTS[i] == savedFont) { fontListIdx = i; break; }

        // Taller panel to fit 4 steps
        m_panel = CCScale9Sprite::create("GJ_square01.png", {0,0,80,80});
        m_panel->setContentSize({390, 380});
        m_panel->setPosition({cx, cy});
        this->addChild(m_panel);

        auto title = CCLabelBMFont::create(getDef().displayName.c_str(), "goldFont.fnt");
        title->setScale(0.65f);
        title->setPosition({cx, cy + 165});
        this->addChild(title);

        // ── Step 1 — message ──
        addStepLabel("Step 1: Type your message", cx, cy + 140);

        inputNode = CCTextInputNode::create(320, 40, "Type here...", "chatFont.fnt");
        inputNode->setPosition({cx, cy + 113});
        inputNode->setDelegate(this); inputNode->setMaxLabelLength(200);
        auto it = customTexts.find(getDef().id);
        if (it != customTexts.end()) inputNode->setString(it->second.c_str());
        this->addChild(inputNode);

        // ── Step 2 — face ──
        addStepLabel("Step 2: Pick a face", cx, cy + 82);

        facePreviewBox = CCNode::create();
        facePreviewBox->setPosition({cx, cy + 52});
        this->addChild(facePreviewBox);

        auto previewBg = CCScale9Sprite::create("GJ_square02.png", {0,0,80,80});
        previewBg->setContentSize({60, 60});
        previewBg->setColor(getDef().portraitColor);
        facePreviewBox->addChild(previewBg);

        faceNumLabel = CCLabelBMFont::create("", "chatFont.fnt");
        faceNumLabel->setScale(0.4f); faceNumLabel->setColor({160,160,160});
        faceNumLabel->setPosition({cx, cy + 26}); this->addChild(faceNumLabel);

        if (getDef().faceIcons.size() > 1) {
            auto fm = CCMenu::create();
            fm->setPosition({cx, cy + 52}); this->addChild(fm);

            auto pa = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
            auto pb = CCMenuItemSpriteExtra::create(
                pa, this, menu_selector(TextInputLayer::onFacePrev));
            pb->setPosition({-48, 0}); fm->addChild(pb);

            auto na = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
            na->setFlipX(true);
            auto nb = CCMenuItemSpriteExtra::create(
                na, this, menu_selector(TextInputLayer::onFaceNext));
            nb->setPosition({48, 0}); fm->addChild(nb);
        }
        updateFacePreview();

        // ── Step 3 — font ──
        addStepLabel("Step 3: Pick a font", cx, cy - 2);

        // Font preview — shows sample text in the chosen font
        fontPreviewLbl = CCLabelBMFont::create("Hello!", AVAILABLE_FONTS[fontListIdx].c_str());
        fontPreviewLbl->setScale(0.55f);
        fontPreviewLbl->setPosition({cx, cy - 26});
        this->addChild(fontPreviewLbl);

        fontNameLbl = CCLabelBMFont::create("", "chatFont.fnt");
        fontNameLbl->setScale(0.38f); fontNameLbl->setColor({160,160,160});
        fontNameLbl->setPosition({cx, cy - 44}); this->addChild(fontNameLbl);
        updateFontLabels();

        auto fontMenu = CCMenu::create();
        fontMenu->setPosition({cx, cy - 26}); this->addChild(fontMenu);

        auto fpa = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
        fpa->setScale(0.75f);
        auto fpb = CCMenuItemSpriteExtra::create(
            fpa, this, menu_selector(TextInputLayer::onFontPrev));
        fpb->setPosition({-75, 0}); fontMenu->addChild(fpb);

        auto fna = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
        fna->setScale(0.75f); fna->setFlipX(true);
        auto fnb = CCMenuItemSpriteExtra::create(
            fna, this, menu_selector(TextInputLayer::onFontNext));
        fnb->setPosition({75, 0}); fontMenu->addChild(fnb);

        // ── Step 4 — keybind ──
        addStepLabel("Step 4: Assign a keybind", cx, cy - 66);

        auto kb = customKeybinds.find(getDef().id);
        std::string kbText = kb != customKeybinds.end() ?
            "Current key: " + getKeyName(kb->second) : "No key set yet";
        keybindLabel = CCLabelBMFont::create(kbText.c_str(), "bigFont.fnt");
        keybindLabel->setScale(0.4f);
        keybindLabel->setPosition({cx, cy - 82}); this->addChild(keybindLabel);

        keybindHint = CCLabelBMFont::create(
            "Click Set Key then press any key", "chatFont.fnt");
        keybindHint->setScale(0.38f); keybindHint->setColor({150,150,150});
        keybindHint->setPosition({cx, cy - 95}); this->addChild(keybindHint);

        auto menu = CCMenu::create();
        menu->setPosition({cx, cy}); this->addChild(menu);

        auto setKeyBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Set Key","goldFont.fnt","GJ_button_04.png",0.6f),
            this, menu_selector(TextInputLayer::onSetKeybind));
        setKeyBtn->setPosition({0, -116}); menu->addChild(setKeyBtn);

        auto saveBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Save","goldFont.fnt","GJ_button_01.png",0.65f),
            this, menu_selector(TextInputLayer::onSave));
        saveBtn->setPosition({58, -152}); menu->addChild(saveBtn);

        auto cancelBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Cancel","goldFont.fnt","GJ_button_06.png",0.65f),
            this, menu_selector(TextInputLayer::onCancel));
        cancelBtn->setPosition({-58, -152}); menu->addChild(cancelBtn);

        this->setTouchEnabled(true);
        return true;
    }

    // Helper to add yellow step labels
    void addStepLabel(const char* text, float x, float y) {
        auto lbl = CCLabelBMFont::create(text, "chatFont.fnt");
        lbl->setScale(0.48f); lbl->setColor({255,255,100});
        lbl->setPosition({x, y}); this->addChild(lbl);
    }

    void updateFacePreview() {
        if (facePreviewSpr) {
            facePreviewSpr->removeFromParentAndCleanup(true);
            facePreviewSpr = nullptr;
        }
        auto& icons = getDef().faceIcons;
        if (!icons.empty()) faceIconNum = icons[faceListIdx];

        auto* spr = tryLoadIcon(faceIconNum);
        if (spr && facePreviewBox) {
            float s = 52.f / std::max(spr->getContentSize().width,
                                      spr->getContentSize().height);
            spr->setScale(s); spr->setPosition({0,0});
            facePreviewBox->addChild(spr);
            facePreviewSpr = spr;
        }
        if (faceNumLabel) {
            auto& ic = getDef().faceIcons;
            faceNumLabel->setString(
                ("Face " + std::to_string(faceListIdx+1) +
                 " / " + std::to_string(ic.size())).c_str());
        }
    }

    void updateFontLabels() {
        const std::string& font = AVAILABLE_FONTS[fontListIdx];

        // Rebuild the preview label in the new font
        if (fontPreviewLbl) {
            fontPreviewLbl->removeFromParentAndCleanup(true);
            fontPreviewLbl = nullptr;
        }
        auto winSize = CCDirector::get()->getWinSize();
        float cx = winSize.width / 2, cy = winSize.height / 2;

        fontPreviewLbl = CCLabelBMFont::create("Hello!", font.c_str());
        fontPreviewLbl->setScale(0.55f);
        fontPreviewLbl->setPosition({cx, cy - 26});
        this->addChild(fontPreviewLbl);

        if (fontNameLbl) {
            std::string display = fontDisplayName(font) +
                " (" + std::to_string(fontListIdx+1) +
                "/" + std::to_string(AVAILABLE_FONTS.size()) + ")";
            fontNameLbl->setString(display.c_str());
        }
    }

    void onFacePrev(CCObject*) {
        int n = (int)getDef().faceIcons.size();
        faceListIdx = (faceListIdx - 1 + n) % n;
        updateFacePreview();
    }
    void onFaceNext(CCObject*) {
        faceListIdx = (faceListIdx + 1) % (int)getDef().faceIcons.size();
        updateFacePreview();
    }
    void onFontPrev(CCObject*) {
        int n = (int)AVAILABLE_FONTS.size();
        fontListIdx = (fontListIdx - 1 + n) % n;
        updateFontLabels();
    }
    void onFontNext(CCObject*) {
        fontListIdx = (fontListIdx + 1) % (int)AVAILABLE_FONTS.size();
        updateFontLabels();
    }

    void registerWithTouchDispatcher() override {
        CCDirector::get()->getTouchDispatcher()
            ->addTargetedDelegate(this, -501, true);
    }
    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        auto lp = this->convertToNodeSpace(touch->getLocation());
        if (m_panel && m_panel->boundingBox().containsPoint(lp)) return false;
        return true;
    }

    void onSetKeybind(CCObject*) {
        g_capturingKeybind = true;
        g_capturingForChar = getDef().id;
        g_keybindLabel     = keybindLabel;
        keybindLabel->setString(">>> Press any key now! <<<");
        keybindLabel->setColor({255,200,0});
        if (keybindHint) keybindHint->setString("(letter, number, or F1-F8)");
    }

    void closeLayer() {
        if (g_capturingKeybind && g_capturingForChar == getDef().id) {
            g_capturingKeybind = false; g_keybindLabel = nullptr;
        }
        inputNode->setDelegate(nullptr);
        if (m_parentPopup) setAllMenusEnabled(true, m_parentPopup);
        this->removeFromParentAndCleanup(true);
    }
    void onSave(CCObject*) {
        customTexts[getDef().id] = std::string(inputNode->getString());
        customFaces[getDef().id] = faceIconNum;
        customFonts[getDef().id] = AVAILABLE_FONTS[fontListIdx];
        auto kb = customKeybinds.find(getDef().id);
        std::string msg = getDef().displayName + " saved!";
        if (kb != customKeybinds.end())
            msg += " Press " + getKeyName(kb->second) + " in-game.";
        else msg += " (Set a keybind to use it.)";
        closeLayer();
        Notification::create(msg, NotificationIcon::Success)->show();
    }
    void onCancel(CCObject*) { closeLayer(); }
};

// ── CharacterSelectLayer ──────────────────────────────────────────────────────
class CharacterSelectLayer : public CCLayer {
public:
    CCScale9Sprite* m_panel = nullptr;

    static CharacterSelectLayer* create() {
        auto ret = new CharacterSelectLayer();
        if (ret->init()) { ret->autorelease(); return ret; }
        delete ret; return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        auto winSize = CCDirector::get()->getWinSize();
        float cx = winSize.width / 2, cy = winSize.height / 2;

        auto bg = CCLayerColor::create({0,0,0,150}, winSize.width, winSize.height);
        this->addChild(bg, -1);

        const float headerH = 52.f, footerH = 8.f;
        const int   n       = (int)CHAR_DEFS.size();
        const float maxH    = winSize.height - 30.f;
        const float btnStep = std::min(36.f, (maxH - headerH - footerH) / n);
        const float panelH  = headerH + btnStep * n + footerH;
        const float panelW  = 300.f;

        m_panel = CCScale9Sprite::create("GJ_square01.png", {0,0,80,80});
        m_panel->setContentSize({panelW, panelH});
        m_panel->setPosition({cx, cy});
        this->addChild(m_panel);

        auto title = CCLabelBMFont::create("Custom Textboxes","goldFont.fnt");
        title->setScale(0.65f);
        title->setPosition({cx, cy + panelH/2 - 18});
        this->addChild(title);

        auto hint = CCLabelBMFont::create("Select a character","chatFont.fnt");
        hint->setScale(0.42f); hint->setColor({200,200,200});
        hint->setPosition({cx, cy + panelH/2 - 34});
        this->addChild(hint);

        auto menu = CCMenu::create();
        menu->setPosition({cx, cy}); this->addChild(menu);

        float startY = ((n-1) * btnStep) / 2.f;
        for (int i = 0; i < n; i++) {
            auto btn = CCMenuItemSpriteExtra::create(
                ButtonSprite::create(CHAR_DEFS[i].displayName.c_str(),
                    "goldFont.fnt","GJ_button_02.png",0.65f),
                this, menu_selector(CharacterSelectLayer::onCharacter));
            btn->setTag(i);
            btn->setPosition({0, startY - i * btnStep});
            menu->addChild(btn);
        }

        auto closeMenu = CCMenu::create();
        float clX = std::max(cx - panelW/2 + 2, 18.f);
        float clY = std::min(cy + panelH/2 - 2, winSize.height - 18.f);
        closeMenu->setPosition({clX, clY});
        this->addChild(closeMenu);

        auto closeBtn = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png"),
            this, menu_selector(CharacterSelectLayer::onClose));
        closeMenu->addChild(closeBtn);

        this->setTouchEnabled(true);
        return true;
    }

    void registerWithTouchDispatcher() override {
        CCDirector::get()->getTouchDispatcher()
            ->addTargetedDelegate(this, -500, true);
    }
    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        auto lp = this->convertToNodeSpace(touch->getLocation());
        if (m_panel && m_panel->boundingBox().containsPoint(lp)) return false;
        return true;
    }
    void onCharacter(CCObject* sender) {
        int idx = static_cast<CCNode*>(sender)->getTag();
        setAllMenusEnabled(false, this);
        CCDirector::get()->getRunningScene()
            ->addChild(TextInputLayer::create(idx, this), 101);
    }
    void onClose(CCObject*) {
        setGameMenusEnabled(true);
        this->removeFromParentAndCleanup(true);
    }
};

// ── Mod button ────────────────────────────────────────────────────────────────
class $modify(MyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        auto btn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("Textboxes","goldFont.fnt","GJ_button_01.png",0.8f),
            this, menu_selector(MyMenuLayer::onMyButton));
        auto menu = CCMenu::create();
        menu->addChild(btn); menu->setPosition({60, 90});
        this->addChild(menu);
        return true;
    }
    void onMyButton(CCObject*) {
        setGameMenusEnabled(false);
        CCDirector::get()->getRunningScene()
            ->addChild(CharacterSelectLayer::create(), 100);
    }
};

// ── Keyboard hook ─────────────────────────────────────────────────────────────
class $modify(MyCCKeyboardDispatcher, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double x) {
        if (down && !repeat) {
            if (g_capturingKeybind) {
                customKeybinds[g_capturingForChar] = key;
                g_capturingKeybind = false;
                if (g_keybindLabel) {
                    g_keybindLabel->setString(
                        ("Current key: " + getKeyName(key)).c_str());
                    g_keybindLabel->setColor({255,255,255});
                    g_keybindLabel = nullptr;
                }
                return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, x);
            }
            for (auto& [charId, keyCode] : customKeybinds) {
                if (keyCode == key) {
                    auto tit = customTexts.find(charId);
                    if (tit != customTexts.end() && !tit->second.empty()) {
                        auto dlg = CharacterDialogLayer::create(charId);
                        if (dlg)
                            CCDirector::get()->getRunningScene()->addChild(dlg, 200);
                    } else {
                        auto* def = findDef(charId);
                        Notification::create(
                            "No text saved for " +
                            (def ? def->displayName : charId) + " yet!",
                            NotificationIcon::Warning)->show();
                    }
                }
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, x);
    }
};
