top = '.'
out = 'build'

def options(ctx):
    ctx.load('pebble_sdk')

def configure(ctx):
    ctx.load('pebble_sdk')

def build(ctx):
    ctx.load('pebble_sdk')

    build_worker = os.path.exists('worker_src')
    binaries = []

    cachedEnv = ctx.env
    for p in ctx.env.TARGET_PLATFORMS:
        ctx.set_env(ctx.all_envs[p])
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.PLATFORM_NAME)
        ctx.pebble_app(
            source=ctx.path.ant_glob('src/**/*.c'),
            target=app_elf,
        )
        binaries.append({'platform': p, 'app_elf': app_elf})
    ctx.set_env(cachedEnv)
    ctx.set_group('bundle')
    ctx.pebble_bundle(binaries=binaries)
