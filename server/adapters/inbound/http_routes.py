from pathlib import Path

from fastapi import APIRouter, Depends, Request
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates
from domain.models import Command
from domain.services import SensorService
from dependencies import get_service, get_api_key_verifier

templates = Jinja2Templates(
    directory=str(Path(__file__).resolve().parent.parent.parent / "templates")
)

router = APIRouter()


@router.get("/", response_class=HTMLResponse)
def dashboard(request: Request, service: SensorService = Depends(get_service)):
    hub = service.get_latest_hub()
    sensors = service.get_latest_sensors()
    history = service.get_history(30)
    hub_id = hub.hub_id if hub else ""
    cmd_logs = service.get_command_logs(hub_id, 20) if hub_id else []
    return templates.TemplateResponse(
        request,
        "dashboard.html",
        context={
            "hub": hub,
            "hub_id": hub_id,
            "sensors": sensors,
            "sensor_history": [r.model_dump() for r in history],
            "command_logs": [log.model_dump() for log in cmd_logs],
        },
    )


@router.get("/api/reports")
def report_history(service: SensorService = Depends(get_service)):
    return [r.model_dump() for r in service.get_history()]


@router.post("/api/hub/{hub_id}/command", dependencies=[Depends(get_api_key_verifier)])
async def send_command(
    hub_id: str,
    commands: list[Command],
    service: SensorService = Depends(get_service),
):
    await service.send_command(hub_id, commands)
    return {"status": "ok"}


@router.post("/api/hub/{hub_id}/status", dependencies=[Depends(get_api_key_verifier)])
async def request_hub_status(
    hub_id: str,
    service: SensorService = Depends(get_service),
):
    await service.send_command(hub_id, [Command(target="", type="HUB_STATUS", payload={})])
    return {"status": "ok", "detail": "HUB_STATUS command sent"}


@router.get("/api/hub/{hub_id}/commands")
def command_history(
    hub_id: str,
    limit: int = 50,
    service: SensorService = Depends(get_service),
):
    return [log.model_dump() for log in service.get_command_logs(hub_id, limit)]


@router.post("/api/hub/{hub_id}/test", dependencies=[Depends(get_api_key_verifier)])
async def send_test(
    hub_id: str,
    target: str,
    led: bool = False,
    service: SensorService = Depends(get_service),
):
    cmd = Command(target=target, type="TEST", payload={"led": led})
    await service.send_command(hub_id, [cmd])
    return {"status": "ok", "cmd_id": cmd.cmd_id, "target": target, "led": led}
