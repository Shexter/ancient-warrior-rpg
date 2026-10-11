#include <iostream>
#include <windows.h>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <conio.h>
#include <string>
#include <ben>

#include "functions.cpp"
#include "npc.cpp"
#include "mission.cpp"
#include "training.cpp"
#include "login.cpp"

using namespace std;


int main(){
    //weapon kode 1x, armor 2x, req item skill3x, req item upgrading 4x, consumable 5x
    int itemID[20]  ={11,13,14,21,22,23,31,32,33,34,35,41,42,43,44,45,46,91,92,93};
    int dropRate[20]={30,0 ,0 ,25,0 ,0 ,40,15,15,7 ,7 ,10,5 ,10,5 ,4 ,3 ,50,50,25};

//    int dropCave[11]={31,32,33,34,35,41,42,43,44,45,46}; //item drop in cave
//    int dropChance[11]={10,5,5,5,5,20,20,10,10,5,5}; //drop chance in cave
    string itemName[20]={   "Iron Sword",   //drop mission/training cave
                            "Fire Claymore",   //kreasi iron sword + phoenix feather + fire dust
                            "Thor's Hammer",//reward mission

                            "Steel Shield", //drop mission/training cave
                            "Diamond Plate",//reward mission
                            "Peace Maker",  //kreasi steel shield + black pearl + Broken Compass + Gold dust

                            "Topaz",        //31 drop mission/training cave
                            "Gold Leaf",    //32 drop mission/training cave
                            "Frozen Tears", //33 drop mission/training cave
                            "Broken Staff", //34 drop mission/training cave
                            "Pure Ice",     //35 drop mission/training cave

                            "Fire Dust",    //41 drop mission/training cave
                            "Turtle Shell", //42 drop mission/training cave

                            "Gold Dust",    //43 drop mission/training cave
                            "Black Pearl",  //44 drop mission/training cave
                            "Broken Compass",//45 drop mission/training cave
                            "Phoenix Feather",//46 drop mission/training cave

                            "Health Potion", //91 health restore
                            "Mana Potion",   //92 mana restore
                            "Eternal Youth"}; //93 hpmp restore (drop item)
    string itemDesc[20]={   "Sharp and Light Sword",
                            "Fiery Great Weapon",
                            "Hammer of Thunder god",

                            "Thick Shield",
                            "Shinny and Pure Plate",
                            "Incredible Destructive Artifact",

                            "Hard Solid Gemstone",
                            "Leaf from OtherWorld",
                            "Tears of Snow Queen",
                            "Piece of Magical Wood",
                            "Frozen white Rock",

                            "Flaming Dust",
                            "Ancient sea creature",

                            "Sprinkle of Magic",
                            "Dark Sphere of the Deep",
                            "Magical Compass",
                            "Ancient Fiery Bird",

                            "Restore Health",
                            "Restore Mana",
                            "Rebirth your Soul"};
    int itemCost[20]={  1000,10000,0,0,
                        5000,20000,0,
                        0,0,0,0,0,
                        0,0,
                        0,0,0,0,
                        1000,5000}; //price 0 is non tradable item
    int itemAttr[20][3]={ {50,0,0},// atk / def / life
                            {1000,100,100},
                            {700,400,0},

                            {0,20,0},
                            {0,200,100},
                            {1000,1000,100},

                            {0,0,0},
                            {0,0,0},
                            {0,0,0},
                            {0,0,0},
                            {0,0,0},

                            {0,0,0},
                            {0,0,0},

                            {0,0,0},
                            {0,0,0},
                            {0,0,0},
                            {0,0,0},

                            {100,0,0},
                            {0,10,0},
                            {0,0,1} };


    int inventoryID[10]={0,0,0,0,0,0,0,0,0,0};
    int inventoryQTY[10]={0,0,0,0,0,0,0,0,0,0};
    int bankID[10]={0,0,0,0,0,0,0,0,0,0};
    int bankQTY[10]={0,0,0,0,0,0,0,0,0,0};
    int recipe1ID=13;
    int recipe1Item[3]={11,46,41};
    int recipe2ID=23;
    int recipe2Item[4]={21,45,44,43};

    //skills can only be used inside dungeon/mission while facing bosses
    string skillName[5]={"Angel WINGS","Meteor FIST","Golden BELL","FROZEN Throne","SUPERNOVA"};
    int skillPoint[5]={10,25,25,40,50};
    string skillDesc[5]={"Greater Healing","Greater Damage","Greater Armor","Deep Freeze and Lower Defense","Ultimate Destruction and Armor Destroyer"};
    //get skill (heal own self, attack=double dmg, life steal(attack)+counter(reflect boss dmg), stun(triple attack turn)+lower boss defs, (halfen boss hp+attack=quintuple dmg+zero boss armor)
    int heroSkill[5]={0,0,0,0,0};
    int completeMission[5]={0,0,0,0,0};

    int weaponID=11, armorID=21;
    bool heroBank=0;
    int heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG,heroDEF,heroLIFE,heroLVL=1,heroEXP=0,heroTNL=0,heroPath=0,heroGold=20000,forgeLvl=9;//heroCCh=0,heroCDmg=0,
    int eqDMG,eqDEF,eqLIFE; //total attr from weapon and armor
    int weaponUP=1,armorUP=1; //equipment level
    //CCh=in % of chance it will occur (will increase along with LVL and equipment)
    //CDmg=in % of heroDMG (will increase along with LVL and equipment)
    //CCh and CDmg will come with the first patch later
    //int statHero[10];//might be useless

    string heroName="\0";

//account sign in, sign up
    srand(time(0));
    int menu1;
    bool m1=1;
    string signMenu[3]={"Sign-In","Sign-Up","EXIT"};
    while(m1){
        cls;
        showTitle();
        menu1=menu(signMenu,3);

        if(menu1==1) m1=signin();
        else if(menu1==2) signup();
        else if(menu1==3) exit(0);
        else if(menu1==0) m1=vip();
        else {
            cout<<"\nChoose 1-3..."; Sleep(2000); continue;
        }
    }

    if(!isFile("game0.dat")) {

        PlaySound("opening.wav",NULL,SND_ASYNC);
        Sleep(2000);
        string story1[7]={"One of the first seven to become a hero, and one of the few who had been chosen by the sacred light",
                        "They called him the bravest of all man, ever since he partook in the Great Quest",
                        "He hunted monsters even if it did not bring him any benefits",
                        "He annihilated the dangerous dungeons that everyone else avoided",
                        "and he also judged heroes and guilds who defied the law",
                        "only he was different...",
                        "Therefore, he will be the one to save the world....."};

        cls;
        showTitle();
        cout<<"\n\n";
        storyLine(story1,7,200);
        Sleep(2000);
        cout<<"\n\n";

        PlaySound("createhero.wav",NULL,SND_ASYNC);
        string story2[4]={"Among all the hero...",
                        "a Fighter is blessed with the Strongest Fist",
                        "a Hunter with the Body Strong as rock",
                        "while a Rogue will have a Balance Mind..."};
        storyLine(story2,4,200);
        Sleep(2000);

        cout<<"Enter Hero name (max 8 character):";
        cin.ignore(100,'\n');
        getline(cin,heroName);
        heroName=heroName.substr(0,8);
        string pathMenu[3]={"FIGHTER","HUNTER","ROGUE"};
        cout<<"WARRIOR PATHS\n";
        heroPath=menu(pathMenu,3);
        createHero(heroPath,heroMaxMP,heroMaxHP,heroDMG,heroDEF,heroLIFE,heroTNL);
        heroHP=heroMaxHP;heroMP=heroMaxMP;
    }
    else{ //retrieve data from files
        file2Hero(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG,heroDEF,heroLIFE,heroLVL,heroEXP,heroTNL,heroPath,heroGold,weaponID,armorID,weaponUP,armorUP,heroBank);
        file2Arrint("game2.dat",inventoryID,10); //array inventory
        file2Arrint("game3.dat",inventoryQTY,10);
        file2Arrint("game4.dat",heroSkill,5);
        file2Arrint("game5.dat",bankID,10); //array bank
        file2Arrint("game6.dat",bankQTY,10);
    }

    PlaySound("Woosh2.wav",NULL,SND_SYNC);

    int playMenu,npcChoice;
    bool play=1;
    while(play){
        cls;
        showTitle();
        heroStat2(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG,heroDEF,heroLIFE,heroLVL,heroEXP,heroTNL,heroPath,heroGold,weaponID,armorID,weaponUP,armorUP,itemID,itemName,itemAttr);
        string mainMenu[5]={"TRAINING","MISSION","N P C","SAVE DATA","EXIT"};
        playMenu=menu(mainMenu,5);
        //cin.ignore(100,'\n');

        switch(playMenu){
            case 1: {
                if(weaponID==0 or armorID==0){
                    warningMsg("Can not enter Training Ground without full Equipment!");
                    break;
                }
                eqDMG=eq2Attr(weaponID,armorID,weaponUP,armorUP,0,itemID,itemAttr);
                eqDEF=eq2Attr(weaponID,armorID,weaponUP,armorUP,1,itemID,itemAttr);
                eqLIFE=eq2Attr(weaponID,armorID,weaponUP,armorUP,2,itemID,itemAttr);
                training(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,
                        heroDMG+eqDMG,
                        heroDEF+eqDEF,
                        heroLIFE+eqLIFE,
                        heroLVL,heroEXP,heroTNL,heroPath,heroGold,inventoryID,inventoryQTY,itemID,itemName,dropRate);
                break;
            }
            case 2: {
                if(weaponID==0 or armorID==0){
                    warningMsg("You need Full Equipment to enter!!");
                    break;
                }
                eqDMG=eq2Attr(weaponID,armorID,weaponUP,armorUP,0,itemID,itemAttr);
                eqDEF=eq2Attr(weaponID,armorID,weaponUP,armorUP,1,itemID,itemAttr);
                eqLIFE=eq2Attr(weaponID,armorID,weaponUP,armorUP,2,itemID,itemAttr);
                mission(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG+eqDMG,heroDEF+eqDEF,heroLIFE+eqLIFE,heroLVL,heroEXP,heroTNL,heroPath,heroGold,weaponID,armorID,weaponUP,armorUP,heroSkill,skillName,skillPoint,skillDesc,itemID,itemName,itemAttr,inventoryID,inventoryQTY,completeMission,dropRate);
                break;
            }
            //npcChoice=menuNPC();
            case 3: int npcChoice;
                    while(1){
                        cls;
                        showTitle();
                        string npcMenu[5]={"Merchant","Blacksmith","GuildMaster","B A N K","EXIT"};
                        cout<<"N P C\n";
                        npcChoice=menu(npcMenu,5);

                        if(npcChoice==1) merchantNPC(itemID,itemName,itemCost,itemAttr,inventoryID,inventoryQTY,heroGold,heroHP);
                        else if(npcChoice==2) blacksmithNPC(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG,heroDEF,heroLIFE,heroGold,heroLVL,heroEXP,heroTNL,heroPath,weaponID,armorID,weaponUP,armorUP,itemID,itemName,itemAttr,inventoryID,inventoryQTY,recipe1ID,recipe1Item,recipe2ID,recipe2Item,forgeLvl);
                        else if(npcChoice==3) guildmasterNPC(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG,heroDEF,heroLIFE,heroLVL,heroEXP,heroTNL,heroPath,heroGold,heroSkill,skillName,skillDesc,itemID,itemName,itemAttr,itemDesc,inventoryID,inventoryQTY);
                        else if(npcChoice==4) bankNPC(heroName,heroGold,heroBank,inventoryID,inventoryQTY,bankID,bankQTY,itemID,itemName,itemAttr);
                        else if(npcChoice==5) break;
                        else {cout<<"Choose 1-5...";Sleep(2000);continue;}
                    }
                    break;
            case 4: hero2File(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG,heroDEF,heroLIFE,heroLVL,heroEXP,heroTNL,heroPath,heroGold,weaponID,armorID,weaponUP,armorUP,heroBank);
                    intarr2File("game2.dat",inventoryID,10);//inventoryID
                    intarr2File("game3.dat",inventoryQTY,10);//inventoryQTY
                    intarr2File("game4.dat",heroSkill,5);//hero skill
                    intarr2File("game5.dat",bankID,10);//bankID
                    intarr2File("game6.dat",bankQTY,10);//bankQTY
                    break;
            case 5: play=0;
                    break;
            case 7: int addLvl;cin>>addLvl;heroLVL+=addLvl;break;
            case 8: int addExp;cin>>addExp;heroEXP+=addExp;break;
            case 9: int addGold;cin>>addGold;heroGold+=addGold;break;
            case 0: cout<<"7:LV 8:XP 9:GOLD";Sleep(500);break;
            default:cout<<"Choose 1-5...";
                    Sleep(2000);
                    break;
        }
    }
}
