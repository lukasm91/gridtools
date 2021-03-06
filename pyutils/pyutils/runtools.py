# -*- coding: utf-8 -*-

import asyncio
import io
import os
import re
import subprocess
import tempfile
import time

from pyutils import env, log


async def _run_async(command, **kwargs):
    process = await asyncio.create_subprocess_exec(
        *command,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.STDOUT,
        env=env.env,
        **kwargs)
    buffer = io.StringIO()

    async def read_output():
        async for line in process.stdout:
            buffer.write(line.decode())

    main_task = asyncio.gather(process.wait(), read_output())

    log_pos = 0

    def log_output():
        nonlocal log_pos
        buffer.seek(log_pos)
        new_output = buffer.read()
        log_pos = buffer.tell()
        if new_output:
            log.debug(f'Output from {command[0]}', new_output)

    async def log_output_periodic():
        while True:
            await asyncio.sleep(10)
            log_output()

    log_task = asyncio.ensure_future(log_output_periodic())

    await main_task
    log_task.cancel()
    log_output()

    buffer.seek(0)
    if process.returncode != 0:
        raise RuntimeError(f'{command[0]} failed with message {buffer.read()}')

    return buffer.read().strip()


def run(command, **kwargs):
    if not command:
        raise ValueError('No command provided')

    log.info('Invoking', ' '.join(command))
    start = time.time()

    loop = asyncio.get_event_loop()
    output = loop.run_until_complete(_run_async(command, **kwargs))

    end = time.time()
    log.info(f'{command[0]} finished in {end - start:.2f}s')
    log.debug(f'{command[0]} output', output)
    return output


def _runscript_file(rundir):
    return os.path.join(rundir, 'run_jobs.sh')


def _sbatch_file(rundir):
    return os.path.join(rundir, 'run.sh')


def _stdout_file(rundir):
    return os.path.join(rundir, f'stdout.out')


def _stderr_file(rundir):
    return os.path.join(rundir, f'stderr.out')


def _generate_runscript(commands):
    code = ''
    for command in commands:
        code += ' '.join(f"'{c}'" for c in command) + '\n'
        code += 'echo "%PYUTILS%$?%RETURNCODE%" >&2\n'
        code += 'echo "%PYUTILS%"\n'
    return code


def _generate_sbatch(rundir, cwd, use_srun, use_mpi_config):
    code = f'#!/bin/bash -l\n'
    code += f'#SBATCH --output={_stdout_file(rundir)}\n'
    code += f'#SBATCH --error={_stderr_file(rundir)}\n'

    for option in env.sbatch_options(use_mpi_config):
        code += f'#SBATCH {option}\n'

    if cwd is None:
        cwd = os.path.abspath(os.getcwd())
    srun = env.srun_command() if use_srun else ''

    code += f'cd {cwd}\n'
    code += f'{srun} bash {_runscript_file(rundir)}\n'
    return code


def _run_sbatch(rundir, commands, cwd, use_srun, use_mpi_config):
    sbatchstr = _generate_sbatch(rundir, cwd, use_srun, use_mpi_config)
    log.debug('Generated sbatch file', sbatchstr)
    with open(_sbatch_file(rundir), 'w') as sbatch:
        sbatch.write(sbatchstr)

    runscriptstr = _generate_runscript(commands)
    log.debug('Generated runscript file', runscriptstr)
    with open(_runscript_file(rundir), 'w') as runscript:
        runscript.write(runscriptstr)

    command = ['sbatch',
               '--wait',
               _sbatch_file(rundir)]
    log.info('Invoking sbatch', ' '.join(command))
    start = time.time()
    result = subprocess.run(command,
                            env=env.env,
                            stderr=subprocess.PIPE,
                            stdout=subprocess.PIPE)
    end = time.time()
    log.info(f'sbatch finished in {end - start:.2f}s')
    if result.returncode != 0 and result.stderr:
        log.error(f'sbatch finished with exit code '
                  f'{result.returncode} and message',
                  result.stderr.decode())
        raise RuntimeError(f'Job submission failed: {result.stderr.decode()}')

    m = re.match(r'Submitted batch job (\d+)', result.stdout.decode())
    if not m:
        log.error(f'Failed parsing sbatch output', result.stdout.decode())
        raise RuntimeError('Job submission failed; sbatch output: '
                           + result.stdout.decode())
    return int(m.group(1))


def _retreive_outputs(rundir, commands, task_id):
    command = ['sacct',
               '--jobs', f'{task_id}',
               '--format', 'jobid,exitcode',
               '--parsable2',
               '--noheader']
    for i in range(1, 7):
        try:
            output = run(command)
        except subprocess.CalledProcessError:
            time.sleep(1)
            continue
        infos = [o.split('|')[1] for o in output.splitlines() if '.batch' in o]
        if len(infos) > 1:
            raise RuntimeError(f'Unexpected sacct output: {output}')
        elif len(infos) == 1:
            exitcode = int(infos[0].split(':')[0])
            if exitcode != 0:
                log.warning('Batch job finished with non-zero exitcode',
                            exitcode)
            break
        time.sleep(i**2)
    else:
        raise RuntimeError('Could not get exit code of job')

    time.sleep(5)

    with open(_stderr_file(rundir), 'r') as outfile:
        stderr = outfile.read()
        log.debug('Raw job stderr', stderr)
        stderr = stderr.split('%RETURNCODE%\n')[:-1]
        stderr, exitcodes = zip(*(o.split('%PYUTILS%') for o in stderr))
        exitcodes = [int(exitcode) for exitcode in exitcodes]

    with open(_stdout_file(rundir), 'r') as outfile:
        stdout = outfile.read()
        log.debug('Raw job stdout', stdout)
        stdout = stdout.split('%PYUTILS%\n')[:-1]

    return list(zip(exitcodes, stdout, stderr))


def _emulate_sbatch(commands, cwd):
    outputs = []
    for command in commands:
        result = subprocess.run(command,
                                env=env.env,
                                cwd=cwd,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)
        outputs.append((result.returncode,
                        result.stdout.decode().strip(),
                        result.stderr.decode().strip()))
    return outputs


def _sbatch(commands, cwd=None, use_srun=True, use_mpi_config=False):
    if env.use_slurm():
        with tempfile.TemporaryDirectory(dir='.') as rundir:
            rundir = os.path.abspath(rundir)
            task = _run_sbatch(rundir, commands, cwd, use_srun, use_mpi_config)
            return _retreive_outputs(rundir, commands, task)
    else:
        return _emulate_sbatch(commands, cwd)


def sbatch(commands, *args, **kwargs):
    outputs = _sbatch(commands, *args, **kwargs)
    failures = ''
    for command, (exitcode, stdout, stderr) in zip(commands, outputs):
        if exitcode != 0:
            failures += (f'Command "{command}" failed with output:\n'
                         f'{stdout}\n{stderr}\n')
    if failures:
        raise RuntimeError(failures)
    return [stdout for _, stdout, _ in outputs]


def sbatch_retry(commands, retries, *args, **kwargs):
    outputs = _sbatch(commands, *args, **kwargs)
    for retry in range(retries):
        successful = sum(exitcode == 0 for exitcode, *_ in outputs)
        if successful == len(outputs):
            break
        if successful < len(outputs) // 2:
            raise RuntimeError('Majority of jobs has failed')

        failed_commands = []
        failed_indices = []
        for i, (command, output) in enumerate(zip(commands, outputs)):
            exitcode, *_ = output
            if exitcode != 0:
                failed_commands.append(command)
                failed_indices.append(i)

        failed_outputs = _sbatch(failed_commands, *args, **kwargs)

        for i, o in zip(failed_indices, failed_outputs):
            outputs[i] = o
    for command, (exitcode, _, stderr) in zip(commands, outputs):
        if exitcode != 0:
            raise RuntimeError(f'Command "{command}" still failed after '
                               f'{retries} retries with output: {stderr}')
    return [stdout for _, stdout, _ in outputs]
