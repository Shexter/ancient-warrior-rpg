
bool mission1(string heroName,int heroMaxMP,int &heroMP,int heroMaxHP,int &heroHP,int heroDMG,int heroDEF,int heroLIFE,int &heroLVL,int &heroEXP,int &heroTNL,int heroPath,int &heroGold,int &weaponID,int &armorID,int &weaponUP,int &armorUP,int heroSkill[],string skillName[],int skillPoint[],string skillDesc[],int itemID[],string itemName[],int itemAttr[][3],int inventoryID[],int inventoryQTY[],int dropRate[]){
    string missionStory1[3]={"Mission ONE :\"The Gorgon\"",
                        "Bring me the head of Medusa...",
                        "...and such artifact never been seen will be yours!\n"};
    Sleep(1000);
    storyLine(missionStory1,3,100);

    int eqDMG=eq2Attr(weaponID,armorID,weaponUP,armorUP,0,itemID,itemAttr);
    int eqDEF=eq2Attr(weaponID,armorID,weaponUP,armorUP,1,itemID,itemAttr);
    int eqLIFE=eq2Attr(weaponID,armorID,weaponUP,armorUP,2,itemID,itemAttr);
    int atkDMG=heroDMG+eqDMG;
    int atkDEF=heroDEF+eqDEF;
    int atkLIFE=heroLIFE+eqLIFE;

    int bossMaxHP=3000,bossHP=3000,bossDMG=250,bossDEF=20,bossLIFE=10;
    string bossName="Medusa";
    //createBoss(1,bossMaxHP,bossHP,bossDMG,bossDEF,bossLIFE)

    int heroReflect=0;
    int turn=1;
    while(1){
        //cls;
        showTitle();
        cout<<"MEDUSA LAIR\n";

        heroStat3(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG,heroDEF,heroLIFE,heroPath,bossName,bossMaxHP,bossHP,bossDMG,bossDEF,bossLIFE);

        //check end condition
        if(bossHP==0){
            warningMsg(bossName+" has been defeated!!",3);
            cout<<"\nEXP Gain  : 10000\n";
            cout<<"GOLD Gain : 25000\n";
            heroEXP+=10000;
            heroGold+=25000;
            levelUP(10000,heroTNL,heroLVL,heroPath);
            cout<<"The Prize as promised...";Sleep(1000);cout<<"Hammer of Thor";
            updBag(14,inventoryID,inventoryQTY,1);
            Sleep(5000);
            return 1;
        }
        else if(heroHP==0){
            weaponID=0;weaponUP=1;armorID=0;armorUP=1; //lost all equipment due to die in battle
            warningMsg(heroName+" has fallen...!! All Equipment destroyed!!");
            return 0;
        }

        //hero turn
        if(turn) {
            heroReflect=0;
            if(turn==1) turn=0;
            else turn--;

            char action;
            cout<<"(A)ttack (S)kill (Q)uit\n";
            cout<<"Select:";
            cin>>action;action=toupper(action);
            if(action=='A') {
                bossHP-=atkDMG-bossDEF;
                bossHP=zeroint(bossHP);

                heroHP+=atkLIFE;
                if(heroHP>heroMaxHP) heroHP=heroMaxHP;
            }
//            else if(action=='H') {
//                heroHP+=heroLIFE;
//                if(heroHP>heroMaxHP) heroHP=heroMaxHP;
//            }
            else if(action=='S') {
                int skillID=menu3(heroSkill,5,skillName,skillPoint,skillDesc);
                if(skillID==6) continue;
                else if(skillID<1 or skillID>5) {
                    cout<<"Invalid Skill";
                    Sleep(2000);
                    continue;
                }
                else{
                    if(heroMP<skillPoint[skillID-1]){
                        cout<<"Mana not enough...";Sleep(2000);continue;
                    }
                    else{
                        heroMP-=skillPoint[skillID-1];
                    }
                }

                if(skillID==1 and heroSkill[0]){
                    cout<<"The Wind shall Heal You ...\n";
                    Sleep(2000);
                    heroHP=heroMaxHP;
                    //heroHP+=(heroMaxHP-heroHP)*(0.1*heroPath);
                    //if(heroHP>heroMaxHP) heroHP=heroMaxHP;
                }
                else if(skillID==2 and heroSkill[1]){
                    cout<<"You Summon the Power of Thunder...\n";
                    Sleep(2000);
                    int skillDMG=heroHP*0.8;
                    bossHP-=skillDMG-bossDEF;
                    bossHP=zeroint(bossHP);
                }
                else if(skillID==3 and heroSkill[2]){
                    cout<<"Golden Bell will Protect You...\n";
                    Sleep(2000);
                    int skillDMG=heroHP*0.8;
                    heroHP+=skillDMG*0.5;
                    bossHP-=skillDMG-bossDEF;
                    bossHP=zeroint(bossHP);
                    heroReflect=1;
                }
                else if(skillID==4 and heroSkill[3]){
                    if(turn==0) turn=5;
                    else {cout<<"You already cast this skill...";Sleep(2000);turn++;continue;}
                    cout<<"You Gather the Power of Frozen Throne...\n";
                    Sleep(2000);
                    int skillDMG=heroDMG*0.5;

                    bossHP-=skillDMG-bossDEF;
                    bossHP=zeroint(bossHP);
                }
            }
            else if(action=='Q') return 0;
        }
        //bosses turn
        else{
            turn=1;
            int rndAction=rand()%10;
            if(rndAction<5){ //50% of normal attack or casting skill
                //normal attack
                if(bossDMG>atkDEF) heroHP-=bossDMG-atkDEF;
                heroHP=zeroint(heroHP);
                bossHP+=bossLIFE;
                if(bossHP>bossMaxHP) bossHP=bossMaxHP;
            }
            else{
                //casting skill
                int rndBlinding=rand()%10;
                if(bossDMG>atkDEF) heroHP-=bossDMG-atkDEF;
                heroHP=zeroint(heroHP);
                if(rndBlinding<6) turn=0; //50% chance of repeating attack
            }
        }
    }
}

bool mission2(string heroName,int heroMaxMP,int &heroMP,int heroMaxHP,int &heroHP,int heroDMG,int heroDEF,int heroLIFE,int &heroLVL,int &heroEXP,int &heroTNL,int heroPath,int &heroGold,int &weaponID,int &armorID,int &weaponUP,int &armorUP,int heroSkill[],string skillName[],int skillPoint[],string skillDesc[],int itemID[],string itemName[],int itemAttr[][3],int dropRate[],int inventoryID[],int inventoryQTY[]){
/*    string missionStory2[3]={"\nMission TWO : \"The Dark Lake\"",
                        "It's a never ending battle against the Hydras...",
                        "...but there's prize for each head you cut!\n"};
    Sleep(1000);
    storyLine(missionStory2,3,100);

    int eqDMG=eq2Attr(weaponID,armorID,weaponUP,armorUP,0,itemID,itemAttr);
    int eqDEF=eq2Attr(weaponID,armorID,weaponUP,armorUP,1,itemID,itemAttr);
    int eqLIFE=eq2Attr(weaponID,armorID,weaponUP,armorUP,2,itemID,itemAttr);
    int atkDMG=heroDMG+eqDMG;
    int atkDEF=heroDEF+eqDEF;
    int atkLIFE=heroLIFE+eqLIFE;

    string bossName;
    int bossRndLevel=rand()%3+1;

    if(bossRndLevel==1) bossName="Earth Hydra";
    else if(bossRndLevel==2) bossName="Fire Hydra";
    else bossName="Black Hydra";

    /////////////////////////////////////////

    int bossMaxHP=bossRndLevel*1000*(completeMission+1);
    int bossHP=bossRndLevel*1000*(completeMission+1);
    int bossDMG=bossRndLevel*50*(completeMission+1);
    int bossDEF=bossRndLevel*50*(completeMission+1);
    int bossLIFE=bossRndLevel*30*(completeMission+1);

    //createBoss(1,bossMaxHP,bossHP,bossDMG,bossDEF,bossLIFE)

    int heroReflect=0;
    int turn=1;
    while(1){
        cls;
        showTitle();
        cout<<"LAKE OF LERNA\n";

        heroStat3(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG,heroDEF,heroLIFE,heroPath,bossName,bossMaxHP,bossHP,bossDMG,bossDEF,bossLIFE);

        //check end condition
        if(bossHP==0){
            warningMsg(bossName+" has been defeated!!",3);
            int gainEXP=0,gainGold=0;
            for(int i=0;i<5;i++){
                gainEXP+=(rand()%4+7)*100;
                gainGold+=(rand()%10+11)*100;
                int dropID=rndDrop(itemID,dropRate);
                updBag(dropID,inventoryID,inventoryQTY,1);
                cout<<"\nGain EXP:"<<gainEXP<<", Gain Gold:"<<gainGold<<", Item:"<<id2Name(dropID,itemID,itemName);
                Sleep(1000);
                //if(rand()%5>1) break;
            }
            heroEXP+=gainEXP;
            levelUP(gainEXP,heroTNL,heroLVL,heroPath);
            heroGold+=gainGold;
            return 1;
        }
        else if(heroHP==0){
            weaponID=0;weaponUP=1;armorID=0;armorUP=1; //lost all equipment due to die in battle
            warningMsg(heroName+" has fallen...!! All Equipment destroyed!!");
            return 0;
        }

        //hero turn
        if(turn) {
            heroReflect=0;
            cout<<turn;system("pause");
            if(turn==1) turn=0;
            else turn--;

            char action;
            cout<<"(A)ttack (S)kill (Q)uit\n";
            cout<<"Select:";
            cin>>action;action=toupper(action);
            if(action=='A') {
                bossHP-=atkDMG-bossDEF;
                bossHP=zeroint(bossHP);

                heroHP+=atkLIFE;
                if(heroHP>heroMaxHP) heroHP=heroMaxHP;
            }
//            else if(action=='H') {
//                heroHP+=heroLIFE;
//                if(heroHP>heroMaxHP) heroHP=heroMaxHP;
//            }
            else if(action=='S') {
                int skillID=menu3(heroSkill,5,skillName,skillPoint,skillDesc);
                if(skillID==6) continue;
                else if(skillID<1 or skillID>5) {
                    cout<<"Invalid Skill";
                    Sleep(2000);
                    continue;
                }
                else{
                    if(heroMP<skillPoint[skillID-1]){
                        cout<<"Mana not enough...";Sleep(2000);continue;
                    }
                    else{
                        heroMP-=skillPoint[skillID-1];
                    }
                }

                if(skillID==1 and heroSkill[0]){
                    cout<<"The Wind shall Heal You ...\n";
                    Sleep(2000);
                    heroHP=heroMaxHP;
                    //heroHP+=(heroMaxHP-heroHP)*(0.1*heroPath);
                    //if(heroHP>heroMaxHP) heroHP=heroMaxHP;
                }
                else if(skillID==2 and heroSkill[1]){
                    cout<<"You Summon the Power of Thunder...\n";
                    Sleep(2000);
                    int skillDMG=heroHP*0.8;
                    bossHP-=skillDMG-bossDEF;
                    bossHP=zeroint(bossHP);
                }
                else if(skillID==3 and heroSkill[2]){
                    cout<<"Golden Bell will Protect You...\n";
                    Sleep(2000);
                    int skillDMG=heroHP*0.8;
                    heroHP+=skillDMG*0.5;
                    bossHP-=skillDMG-bossDEF;
                    bossHP=zeroint(bossHP);
                    heroReflect=1;
                }
                else if(skillID==4 and heroSkill[3]){
                    if(turn==0) turn=5;
                    else {cout<<"You already cast this skill...";Sleep(2000);turn++;heroMP+=skillPoint[skillID-1];continue;}
                    cout<<"You Gather the Power of Frozen Throne...\n";
                    Sleep(2000);
                    int skillDMG=heroDMG*0.5;

                    bossHP-=skillDMG-bossDEF;
                    bossHP=zeroint(bossHP);
                }
            }
            else if(action=='Q') return 0;
        }
        //bosses turn
        else{
            turn=1;
            int rndAction=rand()%10;
            if(rndAction<8){
                //80% chance of melee attack
                if(bossDMG>atkDEF) {
                    heroHP-=bossDMG-atkDEF;
                    cout<<bossName<<" deals "<<bossDMG-atkDEF<<"Damage Point"<<endl;
                    Sleep(3000);
                }
                else{
                    cout<<bossName<<" can not Penetrate your thick Armor,"<<endl;
                    Sleep(3000);
                }
                heroHP=zeroint(heroHP);
            }
            else{
                //20% chance of casting skill : stun hero and break armor
                heroHP-=bossDMG;
                heroHP=zeroint(heroHP);
                turn=0;
                cout<<bossName<<" Penetrate your Armor and deals "<<bossDMG<<" Damage Point"<<endl;
                Sleep(3000);
            }
            bossHP+=bossLIFE; //boss hp regen
            if(bossHP>bossMaxHP) bossHP=bossMaxHP;
        }

    }
    */
}

bool mission3(string heroName,int heroMaxMP,int &heroMP,int heroMaxHP,int &heroHP,int heroDMG,int heroDEF,int heroLIFE,int &heroLVL,int &heroEXP,int &heroTNL,int heroPath,int &heroGold,int &weaponID,int &armorID,int &weaponUP,int &armorUP,int heroSkill[],string skillName[],int skillPoint[],string skillDesc[],int itemID[],string itemName[],int itemAttr[][3],int dropRate[],int inventoryID[],int inventoryQTY[]){
/*    string missionStory2[4]={"\nMission THREE : \"The Twin\"",
                        "...word of wisdom if you will",
                        ".. ..but to listen, pay you must",
                        ". ... ..otherwise this will be your doom ...\n"};
    Sleep(1000);
    storyLine(missionStory2,4,100);

    if(yESNO("Will you pay")){
        string missionStory2a[4]={"\nHave wisdom, for strength alone is not enough",
                            "the creature you're facing is both strong and cunning",
                            "once you enter, the gate will shut...no way out",
                            "watch... for the shadow must not help...\n"};
    }
    Sleep(1000);
    storyLine(missionStory2a,4,100);

    int eqDMG=eq2Attr(weaponID,armorID,weaponUP,armorUP,0,itemID,itemAttr);
    int eqDEF=eq2Attr(weaponID,armorID,weaponUP,armorUP,1,itemID,itemAttr);
    int eqLIFE=eq2Attr(weaponID,armorID,weaponUP,armorUP,2,itemID,itemAttr);

    int atkDMG=heroDMG;     //+eqDMG;
    int atkDEF=heroDEF;     //+eqDEF;
    int atkLIFE=heroLIFE;   //+eqLIFE;

    string boss1Name="Nerubian",boss2Name="Nerubian";

    int boss1MaxHP=5000;
    int boss1HP=5000;
    int boss1DMG=500;
    int boss1DEF=250;
    int boss1LIFE=100;

    int boss2MaxHP=1000;
    int boss2HP=1000;
    int boss2DMG=100;
    int boss2DEF=500;
    int boss2LIFE=100;


    //************************************


    int heroReflect=0;
    int turn=1;
    while(1){
        cls;
        showTitle();
        cout<<"LAKE OF LERNA\n";

        heroStat3(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG,heroDEF,heroLIFE,heroPath,bossName,bossMaxHP,bossHP,bossDMG,bossDEF,bossLIFE);

        //check end condition
        if(bossHP==0){
            warningMsg(bossName+" has been defeated!!",3);
            int gainEXP=0,gainGold=0;
            for(int i=0;i<5;i++){
                gainEXP+=(rand()%4+7)*100;
                gainGold+=(rand()%10+11)*100;
                int dropID=rndDrop(itemID,dropRate);
                updBag(dropID,inventoryID,inventoryQTY,1);
                cout<<"\nGain EXP:"<<gainEXP<<", Gain Gold:"<<gainGold<<", Item:"<<id2Name(dropID,itemID,itemName);
                Sleep(1000);
                //if(rand()%5>1) break;
            }
            heroEXP+=gainEXP;
            levelUP(gainEXP,heroTNL,heroLVL,heroPath);
            heroGold+=gainGold;
            return 1;
        }
        else if(heroHP==0){
            weaponID=0;weaponUP=1;armorID=0;armorUP=1; //lost all equipment due to die in battle
            warningMsg(heroName+" has fallen...!! All Equipment destroyed!!");
            return 0;
        }

        //hero turn
        if(turn) {
            heroReflect=0;
            cout<<turn;system("pause");
            if(turn==1) turn=0;
            else turn--;

            char action;
            cout<<"(A)ttack (S)kill (Q)uit\n";
            cout<<"Select:";
            cin>>action;action=toupper(action);
            if(action=='A') {
                bossHP-=atkDMG-bossDEF;
                bossHP=zeroint(bossHP);

                heroHP+=atkLIFE;
                if(heroHP>heroMaxHP) heroHP=heroMaxHP;
            }
//            else if(action=='H') {
//                heroHP+=heroLIFE;
//                if(heroHP>heroMaxHP) heroHP=heroMaxHP;
//            }
            else if(action=='S') {
                int skillID=menu3(heroSkill,5,skillName,skillPoint,skillDesc);
                if(skillID==6) continue;
                else if(skillID<1 or skillID>5) {
                    cout<<"Invalid Skill";
                    Sleep(2000);
                    continue;
                }
                else{
                    if(heroMP<skillPoint[skillID-1]){
                        cout<<"Mana not enough...";Sleep(2000);continue;
                    }
                    else{
                        heroMP-=skillPoint[skillID-1];
                    }
                }

                if(skillID==1 and heroSkill[0]){
                    cout<<"The Wind shall Heal You ...\n";
                    Sleep(2000);
                    heroHP=heroMaxHP;
                    //heroHP+=(heroMaxHP-heroHP)*(0.1*heroPath);
                    //if(heroHP>heroMaxHP) heroHP=heroMaxHP;
                }
                else if(skillID==2 and heroSkill[1]){
                    cout<<"You Summon the Power of Thunder...\n";
                    Sleep(2000);
                    int skillDMG=heroHP*0.8;
                    bossHP-=skillDMG-bossDEF;
                    bossHP=zeroint(bossHP);
                }
                else if(skillID==3 and heroSkill[2]){
                    cout<<"Golden Bell will Protect You...\n";
                    Sleep(2000);
                    int skillDMG=heroHP*0.8;
                    heroHP+=skillDMG*0.5;
                    bossHP-=skillDMG-bossDEF;
                    bossHP=zeroint(bossHP);
                    heroReflect=1;
                }
                else if(skillID==4 and heroSkill[3]){
                    if(turn==0) turn=5;
                    else {cout<<"You already cast this skill...";Sleep(2000);turn++;heroMP+=skillPoint[skillID-1];continue;}
                    cout<<"You Gather the Power of Frozen Throne...\n";
                    Sleep(2000);
                    int skillDMG=heroDMG*0.5;

                    bossHP-=skillDMG-bossDEF;
                    bossHP=zeroint(bossHP);
                }
            }
            else if(action=='Q') return 0;
        }
        //bosses turn
        else{
            turn=1;
            int rndAction=rand()%10;
            if(rndAction<8){
                //80% chance of melee attack
                if(bossDMG>atkDEF) {
                    heroHP-=bossDMG-atkDEF;
                    cout<<bossName<<" deals "<<bossDMG-atkDEF<<"Damage Point"<<endl;
                    Sleep(3000);
                }
                else{
                    cout<<bossName<<" can not Penetrate your thick Armor,"<<endl;
                    Sleep(3000);
                }
                heroHP=zeroint(heroHP);
            }
            else{
                //20% chance of casting skill : stun hero and break armor
                heroHP-=bossDMG;
                heroHP=zeroint(heroHP);
                turn=0;
                cout<<bossName<<" Penetrate your Armor and deals "<<bossDMG<<" Damage Point"<<endl;
                Sleep(3000);
            }
            bossHP+=bossLIFE; //boss hp regen
            if(bossHP>bossMaxHP) bossHP=bossMaxHP;
        }

    }
    */
}


void mission(string heroName,int heroMaxMP,int &heroMP,int heroMaxHP,int &heroHP,int heroDMG,int heroDEF,int heroLIFE,int &heroLVL,int &heroEXP,int &heroTNL,int heroPath,int &heroGold,int &weaponID,int &armorID,int &weaponUP,int &armorUP,int heroSkill[],string skillName[],int skillPoint[],string skillDesc[],int itemID[],string itemName[],int itemAttr[][3],int inventoryID[],int inventoryQTY[],int completeMission[],int dropRate[]){
    if(heroHP==0){
        cout<<"You don't belong here...BE GONE!!"<<'\n';
        Sleep(3000);
        return;
    }
    if(heroLVL<20){
        cout<<"You need more training young hero...";
        Sleep(3000);
        return;
    }
    string missions[6]={"Dark Queen of Gorgons",
                        "Lake of Lerna",
                        "Coming Soon",
                        "Coming Soon",
                        "Coming Soon",
                        "EXIT"};
    while(1){
        cls;
        showTitle();

        heroStat1(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG,heroDEF,heroLIFE,heroLVL,heroEXP,heroTNL,heroPath,heroGold);
        dispSkill(heroSkill,skillName,skillDesc);
        cout<<"\nM I S S I O N"<<'\n';
        int missionMenu=menu(missions,6);
        if(missionMenu<1 or missionMenu>6) {cout<<"Choose 1-6...";Sleep(2000);continue;}

        if(missionMenu==1){
            if(completeMission[0]>0){
                cout<<"You already cleared this Mission!\n";
                Sleep(2000);
                continue;
            }
            if(mission1(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG,heroDEF,heroLIFE,heroLVL,heroEXP,heroTNL,heroPath,heroGold,weaponID,armorID,weaponUP,armorUP,heroSkill,skillName,skillPoint,skillDesc,itemID,itemName,itemAttr,inventoryID,inventoryQTY,dropRate)){
                completeMission[0]++;
            }
            break;
        }
        else if(missionMenu==2){ errMsg("under construction");continue;
            //if(completeMission[1]>0) {cout<<"You already done this Mission "<<completeMission[1]<<" times";Sleep(2000);}
//            if(mission2(completeMission[1],heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG,heroDEF,heroLIFE,heroLVL,heroEXP,heroTNL,heroPath,heroGold,weaponID,armorID,weaponUP,armorUP,heroSkill,skillName,skillPoint,skillDesc,itemID,itemName,itemAttr,dropRate,inventoryID,inventoryQTY)){
//                completeMission[1]++;
//            }
        }
        else if(missionMenu==3){ errMsg("under construction");continue;
//            if(completeMission[2]>0){
//                cout<<"You already cleared this Mission!\n";
//                Sleep(2000);
//                continue;
//            }
//            if(mission3(heroName,heroMaxMP,heroMP,heroMaxHP,heroHP,heroDMG,heroDEF,heroLIFE,heroLVL,heroEXP,heroTNL,heroPath,heroGold,weaponID,armorID,weaponUP,armorUP,heroSkill,skillName,skillPoint,skillDesc,itemID,itemName,itemAttr,inventoryID,inventoryQTY,dropRate)){
//                completeMission[2]++;
//            }
//            break;
        }
        else break;
    }
}
