# Example tool script: write sRGB hex palettes into Linear Color Curve assets and save them.
# Run: Agent/Scripts/run.sh Agent/Examples/color_curve_palette.py (edit CURVES first).
# Пример скрипта: записать палитры (sRGB hex) в ассеты Linear Color Curve и сохранить их.
# Запуск: Agent/Scripts/run.sh Agent/Examples/color_curve_palette.py (сначала поправить CURVES).
import json

T = 'IluUnrealToolAgent.IluMaterialToolset.'
A = 'editor_toolset.toolsets.asset.AssetTools.'

# Curve asset path -> (sRGB hex colors from time 0 to 1, stepped bands or smooth gradient).
# Путь ассета кривой -> (цвета sRGB hex от времени 0 к 1, ступеньки или плавный градиент).
CURVES = {
    '/Game/Palettes/CC_Example_Day': (['2B3A55', '46597A', '8CC7BF', 'D9B54A', 'EFE3C4', 'D27F6A'], True),
    '/Game/Palettes/CC_Example_Night': (['141A26', '1E2638', '2F5E5C', '54502C', '6E3A30', '3E3456'], False),
}


def lin(c):
    # sRGB -> linear: curves store linear color / кривые хранят линейный цвет.
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def col(h):
    return {'r': lin(int(h[0:2], 16)), 'g': lin(int(h[2:4], 16)), 'b': lin(int(h[4:6], 16)), 'a': 1.0}


def run():
    res = {}
    for path, (colors, stepped) in CURVES.items():
        name = path.rsplit('/', 1)[1]
        ref = {'refPath': path + '.' + name}
        res[name] = execute_tool(T + 'SetLinearColorCurveKeys',
                                 json.dumps({'curve': ref, 'colors': [col(c) for c in colors], 'bStepped': stepped}))['returnValue']
    res['saved'] = execute_tool(A + 'save_assets', json.dumps({'asset_paths': list(CURVES.keys())}))['returnValue']
    return res
