/**
 * Copyright (C) 2021-2026, Alexander Saal <developer@olbaflinx.chm-projects.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QtCore/QLatin1StringView>

namespace olbaflinx::ui::themes {

/**
 * @brief The names of the icons a theme may carry, as compile time constants.
 *
 * Ownership: none. The struct is never instantiated, it only groups the
 * constants. They cost nothing at runtime, so the full catalogue is kept even
 * though the code uses one of them.
 */
struct ThemeManagerIconNames
{
    static constexpr QLatin1StringView Activity = QLatin1StringView("activity");
    static constexpr QLatin1StringView Airplay = QLatin1StringView("airplay");
    static constexpr QLatin1StringView AlertCircle = QLatin1StringView("alert-circle");
    static constexpr QLatin1StringView AlertOctagon = QLatin1StringView("alert-octagon");
    static constexpr QLatin1StringView AlertTriangle = QLatin1StringView("alert-triangle");
    static constexpr QLatin1StringView AlignCenter = QLatin1StringView("align-center");
    static constexpr QLatin1StringView AlignJustify = QLatin1StringView("align-justify");
    static constexpr QLatin1StringView AlignLeft = QLatin1StringView("align-left");
    static constexpr QLatin1StringView AlignRight = QLatin1StringView("align-right");
    static constexpr QLatin1StringView Anchor = QLatin1StringView("anchor");
    static constexpr QLatin1StringView Aperture = QLatin1StringView("aperture");
    static constexpr QLatin1StringView Archive = QLatin1StringView("archive");
    static constexpr QLatin1StringView ArrowDown = QLatin1StringView("arrow-down");
    static constexpr QLatin1StringView ArrowDownCircle = QLatin1StringView("arrow-down-circle");
    static constexpr QLatin1StringView ArrowDownLeft = QLatin1StringView("arrow-down-left");
    static constexpr QLatin1StringView ArrowDownRight = QLatin1StringView("arrow-down-right");
    static constexpr QLatin1StringView ArrowLeft = QLatin1StringView("arrow-left");
    static constexpr QLatin1StringView ArrowLeftCircle = QLatin1StringView("arrow-left-circle");
    static constexpr QLatin1StringView ArrowRight = QLatin1StringView("arrow-right");
    static constexpr QLatin1StringView ArrowRightCircle = QLatin1StringView("arrow-right-circle");
    static constexpr QLatin1StringView ArrowUp = QLatin1StringView("arrow-up");
    static constexpr QLatin1StringView ArrowUpCircle = QLatin1StringView("arrow-up-circle");
    static constexpr QLatin1StringView ArrowUpLeft = QLatin1StringView("arrow-up-left");
    static constexpr QLatin1StringView ArrowUpRight = QLatin1StringView("arrow-up-right");
    static constexpr QLatin1StringView AtSign = QLatin1StringView("at-sign");
    static constexpr QLatin1StringView Award = QLatin1StringView("award");
    static constexpr QLatin1StringView BarChart = QLatin1StringView("bar-chart");
    static constexpr QLatin1StringView BarChart2 = QLatin1StringView("bar-chart-2");
    static constexpr QLatin1StringView Battery = QLatin1StringView("battery");
    static constexpr QLatin1StringView BatteryCharging = QLatin1StringView("battery-charging");
    static constexpr QLatin1StringView Bell = QLatin1StringView("bell");
    static constexpr QLatin1StringView BellOff = QLatin1StringView("bell-off");
    static constexpr QLatin1StringView Bluetooth = QLatin1StringView("bluetooth");
    static constexpr QLatin1StringView Bold = QLatin1StringView("bold");
    static constexpr QLatin1StringView Book = QLatin1StringView("book");
    static constexpr QLatin1StringView BookOpen = QLatin1StringView("book-open");
    static constexpr QLatin1StringView Bookmark = QLatin1StringView("bookmark");
    static constexpr QLatin1StringView Box = QLatin1StringView("box");
    static constexpr QLatin1StringView Briefcase = QLatin1StringView("briefcase");
    static constexpr QLatin1StringView Calendar = QLatin1StringView("calendar");
    static constexpr QLatin1StringView Camera = QLatin1StringView("camera");
    static constexpr QLatin1StringView CameraOff = QLatin1StringView("camera-off");
    static constexpr QLatin1StringView Cast = QLatin1StringView("cast");
    static constexpr QLatin1StringView Check = QLatin1StringView("check");
    static constexpr QLatin1StringView CheckCircle = QLatin1StringView("check-circle");
    static constexpr QLatin1StringView CheckSquare = QLatin1StringView("check-square");
    static constexpr QLatin1StringView ChevronDown = QLatin1StringView("chevron-down");
    static constexpr QLatin1StringView ChevronLeft = QLatin1StringView("chevron-left");
    static constexpr QLatin1StringView ChevronRight = QLatin1StringView("chevron-right");
    static constexpr QLatin1StringView ChevronUp = QLatin1StringView("chevron-up");
    static constexpr QLatin1StringView ChevronsDown = QLatin1StringView("chevrons-down");
    static constexpr QLatin1StringView ChevronsLeft = QLatin1StringView("chevrons-left");
    static constexpr QLatin1StringView ChevronsRight = QLatin1StringView("chevrons-right");
    static constexpr QLatin1StringView ChevronsUp = QLatin1StringView("chevrons-up");
    static constexpr QLatin1StringView Chrome = QLatin1StringView("chrome");
    static constexpr QLatin1StringView Circle = QLatin1StringView("circle");
    static constexpr QLatin1StringView Clipboard = QLatin1StringView("clipboard");
    static constexpr QLatin1StringView Clock = QLatin1StringView("clock");
    static constexpr QLatin1StringView Cloud = QLatin1StringView("cloud");
    static constexpr QLatin1StringView CloudDrizzle = QLatin1StringView("cloud-drizzle");
    static constexpr QLatin1StringView CloudLightning = QLatin1StringView("cloud-lightning");
    static constexpr QLatin1StringView CloudOff = QLatin1StringView("cloud-off");
    static constexpr QLatin1StringView CloudRain = QLatin1StringView("cloud-rain");
    static constexpr QLatin1StringView CloudSnow = QLatin1StringView("cloud-snow");
    static constexpr QLatin1StringView Code = QLatin1StringView("code");
    static constexpr QLatin1StringView Codepen = QLatin1StringView("codepen");
    static constexpr QLatin1StringView Codesandbox = QLatin1StringView("codesandbox");
    static constexpr QLatin1StringView Coffee = QLatin1StringView("coffee");
    static constexpr QLatin1StringView Columns = QLatin1StringView("columns");
    static constexpr QLatin1StringView Command = QLatin1StringView("command");
    static constexpr QLatin1StringView Compass = QLatin1StringView("compass");
    static constexpr QLatin1StringView Copy = QLatin1StringView("copy");
    static constexpr QLatin1StringView CornerDownLeft = QLatin1StringView("corner-down-left");
    static constexpr QLatin1StringView CornerDownRight = QLatin1StringView("corner-down-right");
    static constexpr QLatin1StringView CornerLeftDown = QLatin1StringView("corner-left-down");
    static constexpr QLatin1StringView CornerLeftUp = QLatin1StringView("corner-left-up");
    static constexpr QLatin1StringView CornerRightDown = QLatin1StringView("corner-right-down");
    static constexpr QLatin1StringView CornerRightUp = QLatin1StringView("corner-right-up");
    static constexpr QLatin1StringView CornerUpLeft = QLatin1StringView("corner-up-left");
    static constexpr QLatin1StringView CornerUpRight = QLatin1StringView("corner-up-right");
    static constexpr QLatin1StringView Cpu = QLatin1StringView("cpu");
    static constexpr QLatin1StringView CreditCard = QLatin1StringView("credit-card");
    static constexpr QLatin1StringView Crop = QLatin1StringView("crop");
    static constexpr QLatin1StringView Crosshair = QLatin1StringView("crosshair");
    static constexpr QLatin1StringView Database = QLatin1StringView("database");
    static constexpr QLatin1StringView Delete = QLatin1StringView("delete");
    static constexpr QLatin1StringView Disc = QLatin1StringView("disc");
    static constexpr QLatin1StringView Divide = QLatin1StringView("divide");
    static constexpr QLatin1StringView DivideCircle = QLatin1StringView("divide-circle");
    static constexpr QLatin1StringView DivideSquare = QLatin1StringView("divide-square");
    static constexpr QLatin1StringView DollarSign = QLatin1StringView("dollar-sign");
    static constexpr QLatin1StringView Download = QLatin1StringView("download");
    static constexpr QLatin1StringView DownloadCloud = QLatin1StringView("download-cloud");
    static constexpr QLatin1StringView Dribbble = QLatin1StringView("dribbble");
    static constexpr QLatin1StringView Droplet = QLatin1StringView("droplet");
    static constexpr QLatin1StringView Edit = QLatin1StringView("edit");
    static constexpr QLatin1StringView Edit2 = QLatin1StringView("edit-2");
    static constexpr QLatin1StringView Edit3 = QLatin1StringView("edit-3");
    static constexpr QLatin1StringView ExternalLink = QLatin1StringView("external-link");
    static constexpr QLatin1StringView Eye = QLatin1StringView("eye");
    static constexpr QLatin1StringView EyeOff = QLatin1StringView("eye-off");
    static constexpr QLatin1StringView Facebook = QLatin1StringView("facebook");
    static constexpr QLatin1StringView FastForward = QLatin1StringView("fast-forward");
    static constexpr QLatin1StringView Feather = QLatin1StringView("feather");
    static constexpr QLatin1StringView Figma = QLatin1StringView("figma");
    static constexpr QLatin1StringView File = QLatin1StringView("file");
    static constexpr QLatin1StringView FileMinus = QLatin1StringView("file-minus");
    static constexpr QLatin1StringView FilePlus = QLatin1StringView("file-plus");
    static constexpr QLatin1StringView FileText = QLatin1StringView("file-text");
    static constexpr QLatin1StringView Film = QLatin1StringView("film");
    static constexpr QLatin1StringView Filter = QLatin1StringView("filter");
    static constexpr QLatin1StringView Flag = QLatin1StringView("flag");
    static constexpr QLatin1StringView Folder = QLatin1StringView("folder");
    static constexpr QLatin1StringView FolderMinus = QLatin1StringView("folder-minus");
    static constexpr QLatin1StringView FolderPlus = QLatin1StringView("folder-plus");
    static constexpr QLatin1StringView Framer = QLatin1StringView("framer");
    static constexpr QLatin1StringView Frown = QLatin1StringView("frown");
    static constexpr QLatin1StringView Gift = QLatin1StringView("gift");
    static constexpr QLatin1StringView GitBranch = QLatin1StringView("git-branch");
    static constexpr QLatin1StringView GitCommit = QLatin1StringView("git-commit");
    static constexpr QLatin1StringView GitMerge = QLatin1StringView("git-merge");
    static constexpr QLatin1StringView GitPullRequest = QLatin1StringView("git-pull-request");
    static constexpr QLatin1StringView Github = QLatin1StringView("github");
    static constexpr QLatin1StringView Gitlab = QLatin1StringView("gitlab");
    static constexpr QLatin1StringView Globe = QLatin1StringView("globe");
    static constexpr QLatin1StringView Grid = QLatin1StringView("grid");
    static constexpr QLatin1StringView HardDrive = QLatin1StringView("hard-drive");
    static constexpr QLatin1StringView Hash = QLatin1StringView("hash");
    static constexpr QLatin1StringView Headphones = QLatin1StringView("headphones");
    static constexpr QLatin1StringView Heart = QLatin1StringView("heart");
    static constexpr QLatin1StringView HelpCircle = QLatin1StringView("help-circle");
    static constexpr QLatin1StringView Hexagon = QLatin1StringView("hexagon");
    static constexpr QLatin1StringView Home = QLatin1StringView("home");
    static constexpr QLatin1StringView Image = QLatin1StringView("image");
    static constexpr QLatin1StringView Inbox = QLatin1StringView("inbox");
    static constexpr QLatin1StringView Info = QLatin1StringView("info");
    static constexpr QLatin1StringView Instagram = QLatin1StringView("instagram");
    static constexpr QLatin1StringView Italic = QLatin1StringView("italic");
    static constexpr QLatin1StringView Key = QLatin1StringView("key");
    static constexpr QLatin1StringView Layers = QLatin1StringView("layers");
    static constexpr QLatin1StringView Layout = QLatin1StringView("layout");
    static constexpr QLatin1StringView LifeBuoy = QLatin1StringView("life-buoy");
    static constexpr QLatin1StringView Link = QLatin1StringView("link");
    static constexpr QLatin1StringView Link2 = QLatin1StringView("link-2");
    static constexpr QLatin1StringView Linkedin = QLatin1StringView("linkedin");
    static constexpr QLatin1StringView List = QLatin1StringView("list");
    static constexpr QLatin1StringView Loader = QLatin1StringView("loader");
    static constexpr QLatin1StringView Lock = QLatin1StringView("lock");
    static constexpr QLatin1StringView LogIn = QLatin1StringView("log-in");
    static constexpr QLatin1StringView LogOut = QLatin1StringView("log-out");
    static constexpr QLatin1StringView Mail = QLatin1StringView("mail");
    static constexpr QLatin1StringView Map = QLatin1StringView("map");
    static constexpr QLatin1StringView MapPin = QLatin1StringView("map-pin");
    static constexpr QLatin1StringView Maximize = QLatin1StringView("maximize");
    static constexpr QLatin1StringView Maximize2 = QLatin1StringView("maximize-2");
    static constexpr QLatin1StringView Meh = QLatin1StringView("meh");
    static constexpr QLatin1StringView Menu = QLatin1StringView("menu");
    static constexpr QLatin1StringView MessageCircle = QLatin1StringView("message-circle");
    static constexpr QLatin1StringView MessageSquare = QLatin1StringView("message-square");
    static constexpr QLatin1StringView Mic = QLatin1StringView("mic");
    static constexpr QLatin1StringView MicOff = QLatin1StringView("mic-off");
    static constexpr QLatin1StringView Minimize = QLatin1StringView("minimize");
    static constexpr QLatin1StringView Minimize2 = QLatin1StringView("minimize-2");
    static constexpr QLatin1StringView Minus = QLatin1StringView("minus");
    static constexpr QLatin1StringView MinusCircle = QLatin1StringView("minus-circle");
    static constexpr QLatin1StringView MinusSquare = QLatin1StringView("minus-square");
    static constexpr QLatin1StringView Monitor = QLatin1StringView("monitor");
    static constexpr QLatin1StringView Moon = QLatin1StringView("moon");
    static constexpr QLatin1StringView MoreHorizontal = QLatin1StringView("more-horizontal");
    static constexpr QLatin1StringView MoreVertical = QLatin1StringView("more-vertical");
    static constexpr QLatin1StringView MousePointer = QLatin1StringView("mouse-pointer");
    static constexpr QLatin1StringView Move = QLatin1StringView("move");
    static constexpr QLatin1StringView Music = QLatin1StringView("music");
    static constexpr QLatin1StringView Navigation = QLatin1StringView("navigation");
    static constexpr QLatin1StringView Navigation2 = QLatin1StringView("navigation-2");
    static constexpr QLatin1StringView Octagon = QLatin1StringView("octagon");
    static constexpr QLatin1StringView Package = QLatin1StringView("package");
    static constexpr QLatin1StringView Paperclip = QLatin1StringView("paperclip");
    static constexpr QLatin1StringView Pause = QLatin1StringView("pause");
    static constexpr QLatin1StringView PauseCircle = QLatin1StringView("pause-circle");
    static constexpr QLatin1StringView PenTool = QLatin1StringView("pen-tool");
    static constexpr QLatin1StringView Percent = QLatin1StringView("percent");
    static constexpr QLatin1StringView Phone = QLatin1StringView("phone");
    static constexpr QLatin1StringView PhoneCall = QLatin1StringView("phone-call");
    static constexpr QLatin1StringView PhoneForwarded = QLatin1StringView("phone-forwarded");
    static constexpr QLatin1StringView PhoneIncoming = QLatin1StringView("phone-incoming");
    static constexpr QLatin1StringView PhoneMissed = QLatin1StringView("phone-missed");
    static constexpr QLatin1StringView PhoneOff = QLatin1StringView("phone-off");
    static constexpr QLatin1StringView PhoneOutgoing = QLatin1StringView("phone-outgoing");
    static constexpr QLatin1StringView PieChart = QLatin1StringView("pie-chart");
    static constexpr QLatin1StringView Play = QLatin1StringView("play");
    static constexpr QLatin1StringView PlayCircle = QLatin1StringView("play-circle");
    static constexpr QLatin1StringView Plus = QLatin1StringView("plus");
    static constexpr QLatin1StringView PlusCircle = QLatin1StringView("plus-circle");
    static constexpr QLatin1StringView PlusSquare = QLatin1StringView("plus-square");
    static constexpr QLatin1StringView Pocket = QLatin1StringView("pocket");
    static constexpr QLatin1StringView Power = QLatin1StringView("power");
    static constexpr QLatin1StringView Printer = QLatin1StringView("printer");
    static constexpr QLatin1StringView Radio = QLatin1StringView("radio");
    static constexpr QLatin1StringView RefreshCcw = QLatin1StringView("refresh-ccw");
    static constexpr QLatin1StringView RefreshCw = QLatin1StringView("refresh-cw");
    static constexpr QLatin1StringView Repeat = QLatin1StringView("repeat");
    static constexpr QLatin1StringView Rewind = QLatin1StringView("rewind");
    static constexpr QLatin1StringView RotateCcw = QLatin1StringView("rotate-ccw");
    static constexpr QLatin1StringView RotateCw = QLatin1StringView("rotate-cw");
    static constexpr QLatin1StringView Rss = QLatin1StringView("rss");
    static constexpr QLatin1StringView Save = QLatin1StringView("save");
    static constexpr QLatin1StringView Scissors = QLatin1StringView("scissors");
    static constexpr QLatin1StringView Search = QLatin1StringView("search");
    static constexpr QLatin1StringView Send = QLatin1StringView("send");
    static constexpr QLatin1StringView Server = QLatin1StringView("server");
    static constexpr QLatin1StringView Settings = QLatin1StringView("settings");
    static constexpr QLatin1StringView Share = QLatin1StringView("share");
    static constexpr QLatin1StringView Share2 = QLatin1StringView("share-2");
    static constexpr QLatin1StringView Shield = QLatin1StringView("shield");
    static constexpr QLatin1StringView ShieldOff = QLatin1StringView("shield-off");
    static constexpr QLatin1StringView ShoppingBag = QLatin1StringView("shopping-bag");
    static constexpr QLatin1StringView ShoppingCart = QLatin1StringView("shopping-cart");
    static constexpr QLatin1StringView Shuffle = QLatin1StringView("shuffle");
    static constexpr QLatin1StringView Sidebar = QLatin1StringView("sidebar");
    static constexpr QLatin1StringView SkipBack = QLatin1StringView("skip-back");
    static constexpr QLatin1StringView SkipForward = QLatin1StringView("skip-forward");
    static constexpr QLatin1StringView Slack = QLatin1StringView("slack");
    static constexpr QLatin1StringView Slash = QLatin1StringView("slash");
    static constexpr QLatin1StringView Sliders = QLatin1StringView("sliders");
    static constexpr QLatin1StringView Smartphone = QLatin1StringView("smartphone");
    static constexpr QLatin1StringView Smile = QLatin1StringView("smile");
    static constexpr QLatin1StringView Speaker = QLatin1StringView("speaker");
    static constexpr QLatin1StringView Square = QLatin1StringView("square");
    static constexpr QLatin1StringView Star = QLatin1StringView("star");
    static constexpr QLatin1StringView StopCircle = QLatin1StringView("stop-circle");
    static constexpr QLatin1StringView Sun = QLatin1StringView("sun");
    static constexpr QLatin1StringView Sunrise = QLatin1StringView("sunrise");
    static constexpr QLatin1StringView Sunset = QLatin1StringView("sunset");
    static constexpr QLatin1StringView Table = QLatin1StringView("table");
    static constexpr QLatin1StringView Tablet = QLatin1StringView("tablet");
    static constexpr QLatin1StringView Tag = QLatin1StringView("tag");
    static constexpr QLatin1StringView Target = QLatin1StringView("target");
    static constexpr QLatin1StringView Terminal = QLatin1StringView("terminal");
    static constexpr QLatin1StringView Thermometer = QLatin1StringView("thermometer");
    static constexpr QLatin1StringView ThumbsDown = QLatin1StringView("thumbs-down");
    static constexpr QLatin1StringView ThumbsUp = QLatin1StringView("thumbs-up");
    static constexpr QLatin1StringView ToggleLeft = QLatin1StringView("toggle-left");
    static constexpr QLatin1StringView ToggleRight = QLatin1StringView("toggle-right");
    static constexpr QLatin1StringView Tool = QLatin1StringView("tool");
    static constexpr QLatin1StringView Trash = QLatin1StringView("trash");
    static constexpr QLatin1StringView Trash2 = QLatin1StringView("trash-2");
    static constexpr QLatin1StringView Trello = QLatin1StringView("trello");
    static constexpr QLatin1StringView TrendingDown = QLatin1StringView("trending-down");
    static constexpr QLatin1StringView TrendingUp = QLatin1StringView("trending-up");
    static constexpr QLatin1StringView Triangle = QLatin1StringView("triangle");
    static constexpr QLatin1StringView Truck = QLatin1StringView("truck");
    static constexpr QLatin1StringView Tv = QLatin1StringView("tv");
    static constexpr QLatin1StringView Twitch = QLatin1StringView("twitch");
    static constexpr QLatin1StringView Twitter = QLatin1StringView("twitter");
    static constexpr QLatin1StringView Type = QLatin1StringView("type");
    static constexpr QLatin1StringView Umbrella = QLatin1StringView("umbrella");
    static constexpr QLatin1StringView Underline = QLatin1StringView("underline");
    static constexpr QLatin1StringView Unlock = QLatin1StringView("unlock");
    static constexpr QLatin1StringView Upload = QLatin1StringView("upload");
    static constexpr QLatin1StringView UploadCloud = QLatin1StringView("upload-cloud");
    static constexpr QLatin1StringView User = QLatin1StringView("user");
    static constexpr QLatin1StringView UserCheck = QLatin1StringView("user-check");
    static constexpr QLatin1StringView UserMinus = QLatin1StringView("user-minus");
    static constexpr QLatin1StringView UserPlus = QLatin1StringView("user-plus");
    static constexpr QLatin1StringView UserX = QLatin1StringView("user-x");
    static constexpr QLatin1StringView Users = QLatin1StringView("users");
    static constexpr QLatin1StringView Video = QLatin1StringView("video");
    static constexpr QLatin1StringView VideoOff = QLatin1StringView("video-off");
    static constexpr QLatin1StringView Voicemail = QLatin1StringView("voicemail");
    static constexpr QLatin1StringView Volume = QLatin1StringView("volume");
    static constexpr QLatin1StringView Volume1 = QLatin1StringView("volume-1");
    static constexpr QLatin1StringView Volume2 = QLatin1StringView("volume-2");
    static constexpr QLatin1StringView VolumeX = QLatin1StringView("volume-x");
    static constexpr QLatin1StringView Watch = QLatin1StringView("watch");
    static constexpr QLatin1StringView Wifi = QLatin1StringView("wifi");
    static constexpr QLatin1StringView WifiOff = QLatin1StringView("wifi-off");
    static constexpr QLatin1StringView Wind = QLatin1StringView("wind");
    static constexpr QLatin1StringView X = QLatin1StringView("x");
    static constexpr QLatin1StringView XCircle = QLatin1StringView("x-circle");
    static constexpr QLatin1StringView XOctagon = QLatin1StringView("x-octagon");
    static constexpr QLatin1StringView XSquare = QLatin1StringView("x-square");
    static constexpr QLatin1StringView Youtube = QLatin1StringView("youtube");
    static constexpr QLatin1StringView Zap = QLatin1StringView("zap");
    static constexpr QLatin1StringView ZapOff = QLatin1StringView("zap-off");
    static constexpr QLatin1StringView ZoomIn = QLatin1StringView("zoom-in");
    static constexpr QLatin1StringView ZoomOut = QLatin1StringView("zoom-out");
};

} // namespace olbaflinx::ui::themes
